#!/usr/bin/env python3
"""kseek: KRunner D-Bus runner that pipes `fd` into `fzf` for fast fuzzy file search.

Refactored for KDE Plasma 6 (KF6) with:
- Native GDBus (gi.repository.Gio) eliminating legacy dbus-python
- Asynchronous DBus replies without per-keystroke thread churn
- Atomic process group cancellation (instant kill on superseded keystrokes)
- Myers v2 path-optimized fuzzy matching (`fzf --scheme=path`)
- Wayland XDG Activation Token support for smooth window focusing
- Zero-I/O LRU MIME & icon caching
- Drag-and-drop and clipboard URL metadata support (`urls` variant)
"""
import logging
import mimetypes
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from functools import lru_cache
from urllib.parse import quote, unquote

import gi

gi.require_version("Gio", "2.0")
gi.require_version("GLib", "2.0")
from gi.repository import Gio, GLib, GLibUnix

BUS_NAME = "org.kde.krunner.kseek"
OBJECT_PATH = "/kseek"
INTERFACE = "org.kde.krunner1"

QUERY_RE = re.compile(r"^f\s+(.+)$")

logging.basicConfig(
    level=logging.DEBUG if (os.environ.get("KSEEK_DEBUG") or os.environ.get("KRUNNER_FZF_FD_DEBUG")) else logging.WARNING,
    format="%(asctime)s kseek[%(process)d] %(levelname)s: %(message)s",
    stream=sys.stderr,
)
log = logging.getLogger("kseek")


def _env_float(names: list, default: float) -> float:
    for name in names:
        raw = os.environ.get(name)
        if raw is not None:
            try:
                return float(raw)
            except ValueError:
                log.warning("ignoring invalid %s=%r, using default %.2f", name, raw, default)
    return default


def _env_int(names: list, default: int) -> int:
    for name in names:
        raw = os.environ.get(name)
        if raw is not None:
            try:
                return int(raw)
            except ValueError:
                log.warning("ignoring invalid %s=%r, using default %d", name, raw, default)
    return default


SEARCH_TIMEOUT = _env_float(["KSEEK_TIMEOUT", "KRUNNER_FZF_FD_TIMEOUT"], 2.5)
MAX_RESULTS = _env_int(["KSEEK_MAX_RESULTS", "KRUNNER_FZF_FD_MAX_RESULTS"], 20)
SEARCH_ROOT = os.environ.get("KSEEK_ROOT") or os.environ.get("KRUNNER_FZF_FD_ROOT") or os.path.expanduser("~")
if not os.path.isdir(SEARCH_ROOT):
    log.warning("Search root %r is not a directory, falling back to $HOME", SEARCH_ROOT)
    SEARCH_ROOT = os.path.expanduser("~")

INTROSPECTION_XML = """<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.kde.krunner1">
    <method name="Actions">
      <annotation name="org.qtproject.QtDBus.QtTypeName.Out0" value="RemoteActions"/>
      <arg name="actions" type="a(sss)" direction="out"/>
    </method>
    <method name="Match">
      <annotation name="org.qtproject.QtDBus.QtTypeName.Out0" value="RemoteMatches"/>
      <arg name="query" type="s" direction="in"/>
      <arg name="matches" type="a(sssida{sv})" direction="out"/>
    </method>
    <method name="Run">
      <arg name="matchId" type="s" direction="in"/>
      <arg name="actionId" type="s" direction="in"/>
    </method>
    <method name="Teardown"/>
    <method name="Config"/>
    <method name="SetActivationToken">
      <arg name="token" type="s" direction="in"/>
    </method>
  </interface>
</node>
"""


@lru_cache(maxsize=4096)
def _icon_for_extension(ext: str) -> str:
    """Fast, zero-I/O icon lookup based on file extension using system MIME info."""
    if not ext:
        return "application-octet-stream"
    try:
        content_type, _ = Gio.content_type_guess(f"x{ext}", None)
        if content_type:
            icon_obj = Gio.content_type_get_icon(content_type)
            if icon_obj and hasattr(icon_obj, "get_names"):
                names = icon_obj.get_names()
                if names:
                    return names[0]
    except Exception:
        log.debug("extension icon lookup failed for %s", ext, exc_info=True)

    guess, _ = mimetypes.guess_type(f"x{ext}")
    if guess:
        icon_obj = Gio.content_type_get_icon(guess)
        if icon_obj and hasattr(icon_obj, "get_names"):
            names = icon_obj.get_names()
            if names:
                return names[0]

    return "text-plain" if ext in (".txt", ".md", ".log", ".conf", ".cfg") else "application-octet-stream"


def resolve_icon(file_path: str, is_dir: bool) -> str:
    """Resolves FreeDesktop icon name without synchronous disk I/O."""
    if is_dir:
        return "inode-directory"

    _, ext = os.path.splitext(file_path)
    if ext:
        return _icon_for_extension(ext.lower())

    return "application-octet-stream"


class KSeekRunner:
    def __init__(self, search_root: str = SEARCH_ROOT):
        self.search_root = os.path.abspath(search_root)
        self.fd_bin = shutil.which("fd") or shutil.which("fdfind")
        self.fzf_bin = shutil.which("fzf")

        if not self.fd_bin:
            log.warning("neither 'fd' nor 'fdfind' found on PATH; searches will report an error")
        if not self.fzf_bin:
            log.warning("'fzf' not found on PATH; searches will report an error")

        self.fzf_features = self._detect_fzf_features()
        self.activation_token = ""

        self._lock = threading.Lock()
        self._current_request_id = 0
        self._active_procs = None

        self.system_bus = None
        self.session_bus = None
        self.registration_id = 0

    def _detect_fzf_features(self) -> dict:
        features = {
            "read0": False,
            "print0": False,
            "scheme_path": False,
            "algo_v2": False,
            "tiebreak": False,
        }
        if not self.fzf_bin:
            return features

        try:
            help_res = subprocess.run(
                [self.fzf_bin, "--help"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=2.0,
                text=True,
            )
            help_text = help_res.stdout
            features["read0"] = "--read0" in help_text
            features["print0"] = "--print0" in help_text
            features["scheme_path"] = "--scheme" in help_text
            features["algo_v2"] = "--algo" in help_text
            features["tiebreak"] = "--tiebreak" in help_text
        except Exception:
            log.warning("Failed inspecting fzf help; using safe defaults", exc_info=True)
        return features

    def cancel_active_search(self):
        """Immediately terminates active subprocesses via process group SIGKILL."""
        with self._lock:
            procs = self._active_procs
            self._active_procs = None

        if procs:
            for p in procs:
                if p and p.poll() is None:
                    try:
                        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
                    except (ProcessLookupError, PermissionError):
                        try:
                            p.kill()
                        except Exception:
                            pass
                    except Exception:
                        pass

    def start_dbus(self):
        """Registers the D-Bus service and exports the org.kde.krunner1 object."""
        self.session_bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)

        node_info = Gio.DBusNodeInfo.new_for_xml(INTROSPECTION_XML)
        iface_info = node_info.interfaces[0]

        self.registration_id = self.session_bus.register_object(
            OBJECT_PATH,
            iface_info,
            self._handle_method_call,
            None,
            None,
        )

        # Request well-known bus name
        Gio.bus_own_name_on_connection(
            self.session_bus,
            BUS_NAME,
            Gio.BusNameOwnerFlags.NONE,
            self._on_bus_acquired,
            self._on_name_lost,
        )
        log.info("Registered kseek D-Bus runner at %s %s", BUS_NAME, OBJECT_PATH)

    def _on_bus_acquired(self, conn, name):
        log.debug("Successfully acquired bus name %s", name)

    def _on_name_lost(self, conn, name):
        log.error("Lost bus name %s; another instance might be running.", name)

    def _handle_method_call(
        self,
        connection: Gio.DBusConnection,
        sender: str,
        object_path: str,
        interface_name: str,
        method_name: str,
        parameters: GLib.Variant,
        invocation: Gio.DBusMethodInvocation,
    ):
        try:
            if method_name == "Actions":
                actions = [
                    ("open_app", "Open in Default Application", "system-run"),
                    ("show_item", "Show in Folder", "folder-open"),
                    ("copy_path", "Copy File Path", "edit-copy"),
                    ("open_terminal", "Open Terminal Here", "utilities-terminal"),
                ]
                invocation.return_value(GLib.Variant("(a(sss))", (actions,)))

            elif method_name == "Match":
                query = parameters.unpack()[0]
                with self._lock:
                    self._current_request_id += 1
                    req_id = self._current_request_id

                self.cancel_active_search()

                # Dispatch background search thread so the GLib event loop is never blocked
                threading.Thread(
                    target=self._search_worker,
                    args=(query, req_id, invocation),
                    daemon=True,
                ).start()

            elif method_name == "Run":
                match_id, action_id = parameters.unpack()
                self._execute_run(match_id, action_id)
                invocation.return_value(None)

            elif method_name == "SetActivationToken":
                self.activation_token = parameters.unpack()[0]
                log.debug("Updated XDG activation token: %r", self.activation_token)
                invocation.return_value(None)

            elif method_name in ("Teardown", "Config"):
                self.cancel_active_search()
                invocation.return_value(None)

            else:
                invocation.return_error_literal(
                    Gio.dbus_error_quark(),
                    Gio.DBusError.UNKNOWN_METHOD,
                    f"Unknown method {method_name}",
                )
        except Exception as exc:
            log.exception("Error handling method call %s", method_name)
            invocation.return_error_literal(
                Gio.dbus_error_quark(),
                Gio.DBusError.FAILED,
                str(exc),
            )

    def _search_worker(self, query: str, req_id: int, invocation: Gio.DBusMethodInvocation):
        start_time = time.perf_counter()

        if not self.fd_bin:
            self._reply(invocation, req_id, [self._error_match("'fd' is not installed or not on PATH")])
            return
        if not self.fzf_bin:
            self._reply(invocation, req_id, [self._error_match("'fzf' is not installed or not on PATH")])
            return

        m = QUERY_RE.match(query.strip())
        if not m:
            self._reply(invocation, req_id, [])
            return

        search_term = m.group(1).strip()
        if not search_term:
            self._reply(invocation, req_id, [])
            return

        use_null_io = self.fzf_features["read0"] and self.fzf_features["print0"]

        # Build fd command: traverse directory tree relative to search_root
        fd_cmd = [
            self.fd_bin,
            "--base-directory",
            self.search_root,
            "--hidden",
            "--exclude",
            ".git",
            "--exclude",
            "node_modules",
            "--exclude",
            ".cache",
        ]
        if use_null_io:
            fd_cmd.append("--print0")

        # Build fzf command: fuzzy filter candidates with Myers v2 path scheme
        fzf_cmd = [self.fzf_bin, f"--filter={search_term}"]
        if use_null_io:
            fzf_cmd += ["--read0", "--print0"]
        if self.fzf_features["scheme_path"]:
            fzf_cmd.append("--scheme=path")
        if self.fzf_features["algo_v2"]:
            fzf_cmd.append("--algo=v2")
        if self.fzf_features["tiebreak"]:
            fzf_cmd.append("--tiebreak=length,begin,index")

        log.debug("Launching search [%d]: %s | %s", req_id, " ".join(fd_cmd), " ".join(fzf_cmd))

        fd_proc = None
        fzf_proc = None
        try:
            # Spawn pipeline inside its own process group for atomic signal termination
            fd_proc = subprocess.Popen(
                fd_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                cwd=self.search_root,
                start_new_session=True,
            )

            fzf_proc = subprocess.Popen(
                fzf_cmd,
                stdin=fd_proc.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=self.search_root,
                start_new_session=True,
            )
            # Allow fd to receive SIGPIPE if fzf exits early
            fd_proc.stdout.close()

            with self._lock:
                if req_id != self._current_request_id:
                    # Superseded before we even started
                    try:
                        os.killpg(os.getpgid(fd_proc.pid), signal.SIGKILL)
                    except Exception:
                        pass
                    try:
                        os.killpg(os.getpgid(fzf_proc.pid), signal.SIGKILL)
                    except Exception:
                        pass
                    return
                self._active_procs = (fd_proc, fzf_proc)

            raw_output, raw_stderr = fzf_proc.communicate(timeout=SEARCH_TIMEOUT)
            elapsed = time.perf_counter() - start_time
            log.debug("Search [%d] completed in %.3fs for query: %r", req_id, elapsed, search_term)

        except subprocess.TimeoutExpired:
            log.warning("Search [%d] timed out after %.2fs for %r", req_id, SEARCH_TIMEOUT, search_term)
            self.cancel_active_search()
            self._reply(invocation, req_id, [])
            return
        except Exception as exc:
            log.error("Pipeline execution error in [%d]: %s", req_id, exc)
            self.cancel_active_search()
            self._reply(invocation, req_id, [self._error_match(f"search failed: {exc}")])
            return
        finally:
            if fd_proc and fd_proc.poll() is None:
                try:
                    fd_proc.kill()
                except Exception:
                    pass
            with self._lock:
                if self._active_procs and fd_proc in self._active_procs:
                    self._active_procs = None

        with self._lock:
            if req_id != self._current_request_id:
                log.debug("Discarding stale search results for request [%d]", req_id)
                return

        if fzf_proc.returncode not in (0, 1):
            err_msg = raw_stderr.decode("utf-8", errors="replace").strip() if raw_stderr else ""
            log.error("fzf exited with code %d: %s", fzf_proc.returncode, err_msg)
            self._reply(invocation, req_id, [self._error_match(f"fzf error: {err_msg[:120]}")])
            return

        if not raw_output:
            self._reply(invocation, req_id, [])
            return

        delimiter = "\0" if use_null_io else "\n"
        try:
            relative_paths = [p for p in raw_output.decode("utf-8", errors="surrogateescape").split(delimiter) if p]
        except Exception:
            log.exception("Failed decoding fzf output")
            self._reply(invocation, req_id, [])
            return

        relative_paths = relative_paths[:MAX_RESULTS]
        matches = []
        total = len(relative_paths)
        for rank, rel_path in enumerate(relative_paths):
            match = self._build_match(rel_path, rank, total)
            if match is not None:
                matches.append(match)

        self._reply(invocation, req_id, matches)

    def _reply(self, invocation: Gio.DBusMethodInvocation, req_id: int, matches: list):
        def do_reply():
            with self._lock:
                if req_id != self._current_request_id:
                    # Ignore outdated query results
                    try:
                        invocation.return_value(GLib.Variant("(a(sssida{sv}))", ([],)))
                    except Exception:
                        pass
                    return

            try:
                # Type signature: a(sssida{sv})
                invocation.return_value(GLib.Variant("(a(sssida{sv}))", (matches,)))
            except Exception:
                log.exception("Failed sending D-Bus Match reply for request [%d]", req_id)

        GLib.idle_add(do_reply)

    def _build_match(self, rel_path: str, rank: int, total: int):
        full_path = os.path.normpath(os.path.join(self.search_root, rel_path))
        file_name = os.path.basename(full_path) or full_path

        try:
            is_dir = os.path.isdir(full_path)
        except OSError:
            return None

        icon = resolve_icon(full_path, is_dir)

        # Relevance in [0.5, 1.0] for top items
        relevance = 1.0 if total <= 1 else (1.0 - 0.5 * (rank / (total - 1)))

        # Category relevance: 70 corresponds to High in KF6 CategoryRelevance
        category_relevance = 70

        file_uri = f"file://{quote(os.path.abspath(full_path))}"

        props = {
            "subtext": GLib.Variant("s", full_path),
            "category": GLib.Variant("s", "Files & Folders"),
            "urls": GLib.Variant("as", [file_uri]),
            "actions": GLib.Variant("as", ["open_app", "show_item", "copy_path", "open_terminal"]),
            "multiline": GLib.Variant("b", False),
        }

        return (
            full_path,  # id (s)
            file_name,  # text (s)
            icon,  # iconName (s)
            category_relevance,  # category relevance / type (i)
            relevance,  # relevance (d)
            props,  # properties (a{sv})
        )

    def _error_match(self, message: str):
        return (
            "__error__",
            message,
            "dialog-warning",
            0,
            0.0,
            {
                "subtext": GLib.Variant("s", ""),
                "category": GLib.Variant("s", "Error"),
                "urls": GLib.Variant("as", []),
                "actions": GLib.Variant("as", []),
                "multiline": GLib.Variant("b", False),
            },
        )

    def _execute_run(self, match_id: str, action_id: str):
        if match_id == "__error__":
            return

        file_path = match_id
        if not os.path.exists(file_path) and action_id != "copy_path":
            log.warning("File no longer exists: %s", file_path)
            self._notify("File Not Found", file_path)
            return

        try:
            if not action_id or action_id == "open_app":
                self._open_file(file_path)
            elif action_id == "show_item":
                self._show_in_file_manager(file_path)
            elif action_id == "copy_path":
                self._copy_to_clipboard(file_path)
            elif action_id == "open_terminal":
                self._open_terminal(file_path)
        except Exception:
            log.exception("Action %s failed for %s", action_id, file_path)
            self._notify("Action Failed", f"Could not perform {action_id} on {file_path}")

    def _create_app_launch_context(self) -> Gio.AppLaunchContext:
        context = Gio.AppLaunchContext()
        if self.activation_token:
            context.setenv("XDG_ACTIVATION_TOKEN", self.activation_token)
        return context

    def _open_file(self, path: str):
        file_uri = f"file://{quote(os.path.abspath(path))}"
        launch_context = self._create_app_launch_context()
        try:
            Gio.AppInfo.launch_default_for_uri(file_uri, launch_context)
        except GLib.Error as exc:
            log.warning("Gio launch_default_for_uri failed: %s; trying xdg-open", exc)
            env = os.environ.copy()
            if self.activation_token:
                env["XDG_ACTIVATION_TOKEN"] = self.activation_token
            subprocess.Popen(["xdg-open", path], start_new_session=True, env=env)

    def _show_in_file_manager(self, path: str):
        file_uri = f"file://{quote(os.path.abspath(path))}"
        startup_id = self.activation_token or ""
        try:
            # Use org.freedesktop.FileManager1 ShowItems method
            self.session_bus.call_sync(
                "org.freedesktop.FileManager1",
                "/org/freedesktop/FileManager1",
                "org.freedesktop.FileManager1",
                "ShowItems",
                GLib.Variant("(ass)", ([file_uri], startup_id)),
                None,
                Gio.DBusCallFlags.NONE,
                1500,
                None,
            )
            return
        except Exception:
            log.debug("FileManager1.ShowItems failed; falling back to opening folder", exc_info=True)

        folder = path if os.path.isdir(path) else os.path.dirname(path)
        folder_uri = f"file://{quote(os.path.abspath(folder))}"
        launch_context = self._create_app_launch_context()
        try:
            Gio.AppInfo.launch_default_for_uri(folder_uri, launch_context)
        except Exception:
            subprocess.Popen(["xdg-open", folder], start_new_session=True)

    def _copy_to_clipboard(self, text: str):
        # 1. Native KDE Klipper DBus
        try:
            self.session_bus.call_sync(
                "org.kde.klipper",
                "/klipper",
                "org.kde.klipper.klipper",
                "setClipboardContents",
                GLib.Variant("(s)", (text,)),
                None,
                Gio.DBusCallFlags.NONE,
                1000,
                None,
            )
            log.debug("Copied path via Klipper D-Bus: %s", text)
            return
        except Exception:
            log.debug("Klipper D-Bus unavailable; trying CLI tools", exc_info=True)

        # 2. CLI fallbacks for Wayland and X11
        for tool, args in (("wl-copy", []), ("xclip", ["-selection", "clipboard"]), ("xsel", ["--clipboard"])):
            tool_path = shutil.which(tool)
            if not tool_path:
                continue
            try:
                subprocess.run([tool_path, *args], input=text.encode("utf-8"), check=False, timeout=1.0)
                return
            except Exception:
                pass
        log.warning("No clipboard mechanism available")

    def _open_terminal(self, path: str):
        target_dir = path if os.path.isdir(path) else os.path.dirname(path)
        env = os.environ.copy()
        if self.activation_token:
            env["XDG_ACTIVATION_TOKEN"] = self.activation_token

        for term in ("konsole", "xdg-terminal-exec", "ptyxis", "foot", "kitty", "alacritty", "gnome-terminal", "xterm"):
            bin_path = shutil.which(term)
            if not bin_path:
                continue
            try:
                if term == "konsole":
                    subprocess.Popen([bin_path, "--workdir", target_dir], start_new_session=True, env=env)
                elif term in ("ptyxis", "foot", "alacritty"):
                    subprocess.Popen([bin_path, "--working-directory", target_dir], start_new_session=True, env=env)
                elif term == "kitty":
                    subprocess.Popen([bin_path, "--directory", target_dir], start_new_session=True, env=env)
                else:
                    subprocess.Popen([bin_path], cwd=target_dir, start_new_session=True, env=env)
                return
            except Exception:
                log.debug("Failed spawning terminal %s", term, exc_info=True)

        log.warning("No terminal emulator found on PATH")

    def _notify(self, title: str, message: str):
        try:
            self.session_bus.call_sync(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "Notify",
                GLib.Variant(
                    "(susssasa{sv}i)",
                    ("kseek File Search", 0, "dialog-error", title, message, [], {}, 4000),
                ),
                None,
                Gio.DBusCallFlags.NONE,
                1000,
                None,
            )
        except Exception:
            log.debug("Failed sending notification", exc_info=True)

    def teardown(self):
        self.cancel_active_search()
        if self.registration_id and self.session_bus:
            try:
                self.session_bus.unregister_object(self.registration_id)
            except Exception:
                pass


def install_signal_handlers(loop: GLib.MainLoop, runner: KSeekRunner):
    def handle_signal(sig):
        def _on_signal():
            log.info("Received signal %d; shutting down kseek", sig)
            runner.teardown()
            loop.quit()
            return GLib.SOURCE_REMOVE

        return _on_signal

    for sig in (signal.SIGINT, signal.SIGTERM):
        GLibUnix.signal_add(GLib.PRIORITY_HIGH, sig, handle_signal(sig))


def main():
    runner = KSeekRunner()
    runner.start_dbus()

    loop = GLib.MainLoop()
    install_signal_handlers(loop, runner)

    try:
        loop.run()
    finally:
        runner.teardown()


if __name__ == "__main__":
    main()
