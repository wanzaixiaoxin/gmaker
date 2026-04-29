#!/usr/bin/env python3
"""
Deployment generator for gmaker services.

Usage:
    python tools/deploy-gen/generate.py

Generates:
    - Per-instance directories with config.json
    - Cross-platform run.py (start/stop/status)
    - Shell/Batch helper scripts
"""

import argparse
import json
import os
import sys
from pathlib import Path


_SCRIPT_DIR = Path(__file__).parent.resolve()
_DEFAULT_CONFIG = _SCRIPT_DIR / "deploy.json"
_DEFAULT_ROOT = _SCRIPT_DIR.parent.parent  # assume tools/deploy-gen/ under project root


def load_spec(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def write_json(path: str, data: dict):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")


def write_text(path: str, text: str):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def generate(spec: dict, project_root: str):
    deploy_dir = os.path.abspath(os.path.join(project_root, spec["deploy_dir"]))
    bin_dir = os.path.abspath(os.path.join(project_root, spec["bin_dir"]))
    log_dir = os.path.join(deploy_dir, spec.get("log_dir", "logs"))
    ensure_dir(log_dir)

    base_host = spec["base_host"]
    redis_addr = spec["redis_addr"]
    mysql_dsn = spec["mysql_dsn"]
    services_spec = spec["services"]

    # Collect all generated instances for run.py
    instances = []

    # First pass: compute registry address (assumes single registry for now)
    registry_host = services_spec["registry"].get("host", "0.0.0.0")
    registry_port = services_spec["registry"]["base_port"]
    registry_addr = f"{base_host}:{registry_port}"

    # Collect dbproxy addresses for fallback configs
    dbproxy_addrs = []
    dbproxy_spec = services_spec.get("dbproxy", {})
    for i in range(dbproxy_spec.get("count", 0)):
        port = dbproxy_spec["base_port"] + i
        dbproxy_addrs.append({"host": dbproxy_spec.get("host", base_host), "port": port})

    # Generate per-instance configs and scripts
    for svc_name, svc_cfg in services_spec.items():
        count = svc_cfg["count"]
        base_port = svc_cfg["base_port"]
        host = svc_cfg.get("host", base_host)

        for i in range(1, count + 1):
            node_id = f"{svc_name}-{i}"
            port = base_port + (i - 1)
            inst_dir = os.path.join(deploy_dir, node_id)
            ensure_dir(inst_dir)

            metrics_port = None
            if "metrics_base_port" in svc_cfg:
                metrics_port = svc_cfg["metrics_base_port"] + (i - 1)

            # Build config.json per service type
            config = build_config(
                svc_name=svc_name,
                node_id=node_id,
                host=host,
                port=port,
                metrics_port=metrics_port,
                registry_addr=registry_addr,
                redis_addr=redis_addr,
                mysql_dsn=mysql_dsn,
                dbproxy_addrs=dbproxy_addrs,
                log_dir=log_dir,
            )

            config_path = os.path.join(inst_dir, "config.json")
            write_json(config_path, config)

            # Build start command
            cmd = build_start_command(
                svc_name=svc_name,
                bin_dir=bin_dir,
                config_path="config.json",
                mysql_dsn=mysql_dsn,
                host=host,
                port=port,
            )

            # Instance meta for run.py
            instances.append({
                "name": node_id,
                "dir": os.path.relpath(inst_dir, deploy_dir),
                "cmd": cmd,
                "port": port,
                "metrics_port": metrics_port,
            })

    # 排序启动顺序：基础设施 -> Gateway -> 业务服务
    _start_priority = {"registry": 0, "dbproxy": 1, "gateway": 2}
    instances.sort(key=lambda x: (_start_priority.get(x["name"].split("-")[0], 3), x["name"]))

    # Generate run.py
    run_py = build_run_py(instances, deploy_dir, bin_dir)
    write_text(os.path.join(deploy_dir, "run.py"), run_py)

    # Generate start-all.sh / stop-all.sh for convenience
    sh_scripts = build_shell_scripts(instances)
    write_text(os.path.join(deploy_dir, "start-all.sh"), sh_scripts["start"])
    write_text(os.path.join(deploy_dir, "stop-all.sh"), sh_scripts["stop"])
    write_text(os.path.join(deploy_dir, "status.sh"), sh_scripts["status"])

    # Generate Windows batch scripts
    bat_scripts = build_bat_scripts(instances)
    write_text(os.path.join(deploy_dir, "start-all.bat"), bat_scripts["start"])
    write_text(os.path.join(deploy_dir, "stop-all.bat"), bat_scripts["stop"])
    write_text(os.path.join(deploy_dir, "status.bat"), bat_scripts["status"])

    # Summary
    print(f"Generated deployment at: {deploy_dir}")
    print(f"Instances: {len(instances)}")
    for inst in instances:
        print(f"  {inst['name']:20s} port={inst['port']:<5d} metrics={inst['metrics_port'] or 'N/A'}")
    print(f"\nUsage:")
    print(f"  cd {os.path.relpath(deploy_dir, project_root)}")
    print(f"  python run.py start        # start all")
    print(f"  python run.py stop         # stop all")
    print(f"  python run.py status       # show status")
    print(f"  python run.py start biz-1  # start specific instance")


def build_config(
    svc_name: str,
    node_id: str,
    host: str,
    port: int,
    metrics_port: int | None,
    registry_addr: str,
    redis_addr: str,
    mysql_dsn: str,
    dbproxy_addrs: list,
    log_dir: str,
) -> dict:
    log_file = os.path.join(log_dir, f"{node_id}.log").replace("\\", "/")

    if svc_name == "registry":
        # Registry uses CLI flags primarily; config is for reference
        return {
            "service": {
                "service_type": "registry",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
            },
            "network": {
                "host": host,
                "port": port,
                "max_connections": 1000,
            },
        }

    if svc_name == "dbproxy":
        return {
            "service": {
                "service_type": "dbproxy",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
            },
            "network": {
                "host": host,
                "port": port,
            },
            "discovery": {
                "type": "registry",
                "addrs": [registry_addr],
            },
            "mysql": {
                "dsn": mysql_dsn,
                "max_open_conn": 20,
                "max_idle_conn": 5,
                "conn_max_lifetime_sec": 3600,
            },
        }

    if svc_name == "login":
        return {
            "service": {
                "service_type": "login",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
                "metrics_addr": f":{metrics_port}",
            },
            "network": {
                "host": host,
                "port": port,
            },
            "discovery": {
                "type": "registry",
                "addrs": [registry_addr],
            },
            "redis": {
                "addrs": [redis_addr],
                "password": "",
                "pool_size": 20,
            },
            "dbproxy": {
                "nodes": dbproxy_addrs,
            },
        }

    if svc_name == "biz":
        return {
            "service": {
                "service_type": "biz",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
                "metrics_addr": f":{metrics_port}",
            },
            "network": {
                "host": host,
                "port": port,
            },
            "discovery": {
                "type": "registry",
                "addrs": [registry_addr],
            },
            "redis": {
                "addrs": [redis_addr],
                "password": "",
                "pool_size": 20,
            },
        }

    if svc_name == "chat":
        return {
            "service": {
                "service_type": "chat",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
                "metrics_addr": f":{metrics_port}",
            },
            "network": {
                "host": host,
                "port": port,
                "max_connections": 5000,
            },
            "discovery": {
                "type": "registry",
                "addrs": [registry_addr],
            },
            "redis": {
                "addrs": [redis_addr],
                "password": "",
                "pool_size": 20,
            },
            "dbproxy": {
                "nodes": dbproxy_addrs,
            },
        }

    if svc_name == "gateway":
        return {
            "service": {
                "service_type": "gateway",
                "node_id": node_id,
                "log_level": "info",
                "log_file": log_file,
                "metrics_addr": f":{metrics_port}",
            },
            "network": {
                "port": port,
                "websocket_port": port + 2,
                "max_connections": 10000,
            },
            "discovery": {
                "type": "registry",
                "addrs": [registry_addr],
            },
            "upstream": {
                "services": ["biz", "chat", "login"],
            },
            "coalescer_interval_ms": 16,
            "security": {
                "master_key_hex": "",
                "replay_window_seconds": 300,
            },
        }

    raise ValueError(f"Unknown service type: {svc_name}")


def build_start_command(svc_name: str, bin_dir: str, config_path: str, mysql_dsn: str, host: str, port: int) -> list:
    if svc_name == "registry":
        return [os.path.join(bin_dir, "registry-go"), "-listen", f"{host}:{port}", "-store", "memory", "-log-level", "info"]
    if svc_name == "dbproxy":
        return [os.path.join(bin_dir, "dbproxy-go"), "-config", config_path, "-mysql", mysql_dsn]
    if svc_name == "gateway":
        return [os.path.join(bin_dir, "gateway-cpp"), "--config", config_path]
    # login, biz, chat
    return [os.path.join(bin_dir, f"{svc_name}-go"), "-config", config_path]


def build_run_py(instances: list, deploy_dir: str, bin_dir: str) -> str:
    inst_json = repr(instances)
    # Use relative paths inside run.py so it works when moved
    return f'''#!/usr/bin/env python3
"""
Unified service launcher for gmaker deployment.

Usage:
    python run.py start [instance_name]     # start all or specific instance
    python run.py stop [instance_name]      # stop all or specific instance
    python run.py status                    # show running status
    python run.py restart [instance_name]   # restart all or specific instance
"""

import json
import os
import subprocess
import sys
import time
from pathlib import Path

DEPLOY_DIR = Path(__file__).parent.resolve()
BIN_DIR = Path(r"{bin_dir}").resolve()
PID_DIR = DEPLOY_DIR / ".pids"
PID_DIR.mkdir(exist_ok=True)

INSTANCES = {inst_json}


def get_pid_file(name: str) -> Path:
    return PID_DIR / f"{{name}}.pid"


def save_pid(name: str, pid: int):
    get_pid_file(name).write_text(str(pid))


def read_pid(name: str) -> int | None:
    pf = get_pid_file(name)
    if not pf.exists():
        return None
    try:
        return int(pf.read_text().strip())
    except ValueError:
        return None


def remove_pid(name: str):
    pf = get_pid_file(name)
    if pf.exists():
        pf.unlink()


def is_running(pid: int) -> bool:
    if sys.platform == "win32":
        try:
            import ctypes
            kernel = ctypes.windll.kernel32
            handle = kernel.OpenProcess(1, False, pid)
            if handle:
                kernel.CloseHandle(handle)
                return True
            return False
        except Exception:
            return False
    else:
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False


def start_instance(inst: dict):
    name = inst["name"]
    pid = read_pid(name)
    if pid and is_running(pid):
        print(f"[{{name}}] already running (pid={{pid}})")
        return

    inst_dir = DEPLOY_DIR / inst["dir"]
    cmd = [str(c) for c in inst["cmd"]]
    # Make binary path absolute
    cmd[0] = str(BIN_DIR / Path(cmd[0]).name)

    env = os.environ.copy()
    # Ensure binary can find shared libs on Windows
    env["PATH"] = str(BIN_DIR) + os.pathsep + env.get("PATH", "")

    log_path = DEPLOY_DIR / "logs" / f"{{name}}.console.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "a", encoding="utf-8") as log_f:
        proc = subprocess.Popen(cmd, cwd=str(inst_dir), env=env, stdout=log_f, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0)
        save_pid(name, proc.pid)
        print(f"[{{name}}] started (pid={{proc.pid}}) port={{inst['port']}}")
    time.sleep(0.3)


def stop_instance(inst: dict):
    name = inst["name"]
    pid = read_pid(name)
    if not pid:
        print(f"[{{name}}] not tracked")
        return
    if not is_running(pid):
        print(f"[{{name}}] not running (stale pid)")
        remove_pid(name)
        return

    if sys.platform == "win32":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)
    else:
        try:
            os.kill(pid, 15)
            time.sleep(0.5)
            if is_running(pid):
                os.kill(pid, 9)
        except ProcessLookupError:
            pass
    remove_pid(name)
    print(f"[{{name}}] stopped")


def show_status():
    print(f"{{'INSTANCE':<20}} {{'PORT':<6}} {{'METRICS':<8}} {{'PID':<8}} {{'STATUS'}}")
    print("-" * 60)
    for inst in INSTANCES:
        name = inst["name"]
        pid = read_pid(name)
        status = "running" if pid and is_running(pid) else "stopped"
        metrics = inst.get("metrics_port") or "N/A"
        print(f"{{name:<20}} {{inst['port']:<6}} {{metrics:<8}} {{pid or 'N/A':<8}} {{status}}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    action = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else None

    selected = INSTANCES
    if target:
        selected = [i for i in INSTANCES if i["name"] == target]
        if not selected:
            print(f"Unknown instance: {{target}}")
            sys.exit(1)

    if action == "start":
        for inst in selected:
            start_instance(inst)
    elif action == "stop":
        for inst in reversed(selected):
            stop_instance(inst)
    elif action == "restart":
        for inst in reversed(selected):
            stop_instance(inst)
        time.sleep(0.5)
        for inst in selected:
            start_instance(inst)
    elif action == "status":
        show_status()
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
'''


def build_shell_scripts(instances: list) -> dict:
    lines_start = ["#!/usr/bin/env bash", "set -e", 'cd "$(dirname "$0")"']
    lines_stop = ["#!/usr/bin/env bash", 'cd "$(dirname "$0")"']
    lines_status = ["#!/usr/bin/env bash", 'cd "$(dirname "$0")"', 'echo "INSTANCE             PORT   PID     STATUS"', 'echo "-------------------------------------------"']

    for inst in instances:
        name = inst["name"]
        dir_name = inst["dir"]
        cmd = " ".join(f'"{str(c).replace(chr(92), "/")}"' for c in inst["cmd"])
        lines_start.append(f'echo "Starting {name}..."')
        lines_start.append(f'cd "{dir_name}" && nohup {cmd} > /dev/null 2>&1 &')
        lines_start.append(f'echo $! > .pids/{name}.pid')
        lines_start.append("cd ..")
        lines_start.append("")

        lines_stop.append(f'if [ -f .pids/{name}.pid ]; then kill $(cat .pids/{name}.pid) 2>/dev/null; rm -f .pids/{name}.pid; echo "Stopped {name}"; fi')

        port = inst['port']
        lines_status.append(f'if [ -f .pids/{name}.pid ] && kill -0 $(cat .pids/{name}.pid) 2>/dev/null; then echo "{name:<20} {port:<6} $(cat .pids/{name}.pid)   running"; else echo "{name:<20} {port:<6} N/A     stopped"; fi')

    return {
        "start": "\n".join(lines_start) + "\n",
        "stop": "\n".join(lines_stop) + "\n",
        "status": "\n".join(lines_status) + "\n",
    }


def build_bat_scripts(instances: list) -> dict:
    lines_start = ["@echo off", "setlocal EnableDelayedExpansion", "cd /d %~dp0"]
    lines_stop = ["@echo off", "cd /d %~dp0"]
    lines_status = ["@echo off", "cd /d %~dp0", "echo INSTANCE             PORT   PID     STATUS", "echo ------------------------------------------"]

    for inst in instances:
        name = inst["name"]
        dir_name = inst["dir"]
        # Convert forward slashes for Windows cmd
        cmd_parts = []
        for c in inst["cmd"]:
            s = str(c).replace("/", "\\")
            if " " in s:
                s = f'"{s}"'
            cmd_parts.append(s)
        cmd = " ".join(cmd_parts)

        lines_start.append(f'echo Starting {name}...')
        lines_start.append(f'cd "{dir_name}"')
        lines_start.append(f'start "" /B {cmd}')
        lines_start.append(f'for /f "tokens=2" %%a in (\'tasklist /fi "imagename eq {os.path.basename(inst["cmd"][0])}.exe" /fo list ^| findstr "PID:"\') do echo %%a > ..\\.pids\\{name}.pid')
        lines_start.append("cd ..")
        lines_start.append("")

        lines_stop.append(f'if exist .pids\\{name}.pid (')
        lines_stop.append(f'  for /f %%a in (.pids\\{name}.pid) do taskkill /PID %%a /T /F >nul 2>&1')
        lines_stop.append(f'  del .pids\\{name}.pid')
        lines_stop.append(f'  echo Stopped {name}')
        lines_stop.append(")")

        lines_status.append(f'if exist .pids\\{name}.pid (')
        lines_status.append(f'  for /f %%a in (.pids\\{name}.pid) do echo {name:<20} {inst["port"]:<6} %%a    running')
        lines_status.append(") else (")
        lines_status.append(f'  echo {name:<20} {inst["port"]:<6} N/A     stopped')
        lines_status.append(")")

    return {
        "start": "\n".join(lines_start) + "\n",
        "stop": "\n".join(lines_stop) + "\n",
        "status": "\n".join(lines_status) + "\n",
    }


def main():
    parser = argparse.ArgumentParser(description="Generate gmaker deployment")
    parser.add_argument("-c", "--config", type=Path, default=_DEFAULT_CONFIG, help="Deployment spec JSON file")
    parser.add_argument("--root", type=Path, default=_DEFAULT_ROOT, help="Project root directory")
    args = parser.parse_args()

    project_root = args.root.resolve()
    spec_path = args.config.resolve()

    if not spec_path.exists():
        print(f"Spec file not found: {spec_path}")
        sys.exit(1)

    spec = load_spec(str(spec_path))
    generate(spec, str(project_root))


if __name__ == "__main__":
    main()
