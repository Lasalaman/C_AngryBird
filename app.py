"""
憤怒鳥 Flask 後端：透過 subprocess 呼叫 C 語言物理引擎（angrybird_sim.exe）

架構：
  瀏覽器  --HTTP JSON-->  Flask (/api/launch)
                              |
                              | subprocess.run(input=JSON 字串)
                              v
                         angrybird_sim.exe
                              |
                              | stdout 輸出軌跡 JSON
                              v
                         Flask jsonify 回傳前端
"""

from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path
from typing import Any

from flask import Flask, jsonify, render_template, request

# ---------------------------------------------------------------------------
# 設定
# ---------------------------------------------------------------------------

# 專案根目錄（app.py 所在目錄），用來定位 C 執行檔的絕對路徑
PROJECT_ROOT = Path(__file__).resolve().parent

# C 物理引擎執行檔（請先以 make / gcc 編譯產生 angrybird_sim.exe）
SIM_EXECUTABLE = PROJECT_ROOT / "angrybird_sim.exe"

# subprocess 最長等待秒數，避免 C 程式異常卡住 Python 程序
SIM_TIMEOUT_SEC = 10.0

# 預設關卡 1：右側綠豬 + 木箱（座標與 C 引擎 json_io 格式一致）
# kind: 1 = 豬 (PIG), 2 = 結構障礙物 (OBSTACLE)
DEFAULT_LEVEL: dict[str, Any] = {
    "level_id": 1,
    "bounds": {"min_x": 0, "min_y": 0, "max_x": 1280, "max_y": 720},
    "gravity": 980.0,
    "time_step": 1.0 / 60.0,
    "bird_radius": 16.0,
    "spawn": {"start_x": 120.0, "start_y": 348.0},
    "obstacles": [
        {
            "id": 1,
            "kind": 1,
            "material": 0,
            "x": 980,
            "y": 580,
            "width": 52,
            "height": 52,
            "hit_points": 100,
        },
        {
            "id": 2,
            "kind": 2,
            "material": 1,
            "x": 860,
            "y": 630,
            "width": 70,
            "height": 28,
            "hit_points": 40,
        },
        {
            "id": 3,
            "kind": 2,
            "material": 1,
            "x": 930,
            "y": 600,
            "width": 70,
            "height": 28,
            "hit_points": 40,
        },
        {
            "id": 4,
            "kind": 2,
            "material": 1,
            "x": 900,
            "y": 565,
            "width": 32,
            "height": 70,
            "hit_points": 40,
        },
    ],
}

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)

# Flask 端遊戲工作階段（C 每次 subprocess 結束後由回傳 JSON 同步）
# birds：剩餘待發射佇列；obstacles：目前關卡障礙物狀態
_game_session: dict[str, Any] = {}

LEVEL_BIRD_QUEUE_CAPACITY = 3


def _default_bounds() -> dict[str, float]:
    b = DEFAULT_LEVEL["bounds"]
    return {
        "min_x": b["min_x"],
        "min_y": b["min_y"],
        "max_x": b["max_x"],
        "max_y": b["max_y"],
    }


def reset_game_session() -> dict[str, Any]:
    """呼叫 C 核心 reset：malloc 關卡 + 3 鳥 Queue，並更新 Flask 工作階段。"""
    global _game_session

    payload: dict[str, Any] = {
        "action": "reset",
        "gravity": DEFAULT_LEVEL["gravity"],
        "time_step": DEFAULT_LEVEL["time_step"],
        "min_x": DEFAULT_LEVEL["bounds"]["min_x"],
        "min_y": DEFAULT_LEVEL["bounds"]["min_y"],
        "max_x": DEFAULT_LEVEL["bounds"]["max_x"],
        "max_y": DEFAULT_LEVEL["bounds"]["max_y"],
    }

    result = run_physics_engine(payload)
    _game_session = {
        "remaining_birds": result.get("remaining_birds", LEVEL_BIRD_QUEUE_CAPACITY),
        "remaining_pigs": result.get("remaining_pigs", 1),
        "total_birds": result.get("total_birds", 3),
        "birds": result.get("birds", []),
        "obstacles": result.get("obstacles", DEFAULT_LEVEL["obstacles"]),
        "game_status": result.get("game_status", "playing"),
    }
    return result


@app.after_request
def add_cors_headers(response):
    """開發用：允許前端從其他 port（如 Live Server）呼叫 API。"""
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS"
    response.headers["Access-Control-Allow-Headers"] = "Content-Type"
    return response


def run_physics_engine(payload: dict[str, Any]) -> dict[str, Any]:
    """
    啟動 C 執行檔，以 stdin/stdout 交換 JSON。

    Process 資料流說明（Python 與 C 的安全協作）：
    ------------------------------------------------------------------
    1. Python 將 dict 序列化為 JSON 字串，並在結尾加上 '\\n'。
       C 端 main() 使用 fgets() 讀取「一行」，換行符號標記輸入結束。

    2. subprocess.run(..., input=...) 會：
       - 建立獨立子行程（child process），與 Flask 記憶體空離；
       - 將 input 字串寫入子行程的 stdin 管道（pipe）；
       - 子行程結束後關閉管道，避免子行程永久等待輸入。

    3. capture_output=True 會把子行程 stdout/stderr 導向管道，
       由父行程（Python）讀取為 completed.stdout（bytes 或 str）。
       不會與 Flask 自己的 stdout 混在一起。

    4. text=True 表示以 UTF-8 字串傳遞，省去手動 decode。
       若 C 程式輸出非 UTF-8，需改為 bytes 再 decode。

    5. timeout 限制最長執行時間；逾時時 subprocess 會終止子行程，
       避免殭屍行程或無限阻塞。

    6. 不使用 shell=True，直接執行 .exe 路徑，降低命令注入風險。
    ------------------------------------------------------------------
    """
    if not SIM_EXECUTABLE.is_file():
        raise FileNotFoundError(
            f"找不到物理引擎：{SIM_EXECUTABLE}，請先編譯 angrybird_sim.exe"
        )

    # 封裝給 C 的 stdin：必須含 "angle" 與 "velocity" 鍵（見 json_io.c）
    stdin_payload = json.dumps(payload, separators=(",", ":")) + "\n"

    logger.info("呼叫 C 引擎：%s", SIM_EXECUTABLE)
    logger.debug("stdin >>> %s", stdin_payload.strip())

    completed = subprocess.run(
        [str(SIM_EXECUTABLE)],  # 參數列表，不經 shell 解析
        input=stdin_payload,
        capture_output=True,
        text=True,
        timeout=SIM_TIMEOUT_SEC,
        cwd=str(PROJECT_ROOT),  # 子行程工作目錄設在專案根
        check=False,  # 非零結束碼由本函式自行判斷
    )

    stdout_text = (completed.stdout or "").strip()
    stderr_text = (completed.stderr or "").strip()

    logger.debug("stdout <<< %s", stdout_text)
    if stderr_text:
        logger.warning("stderr <<< %s", stderr_text)

    if completed.returncode != 0:
        raise RuntimeError(
            f"C 引擎結束碼 {completed.returncode}；stderr={stderr_text or '(空)'}"
        )

    if not stdout_text:
        raise RuntimeError("C 引擎未輸出任何 stdout 資料")

    try:
        result = json.loads(stdout_text)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"無法解析 C 引擎輸出為 JSON：{stdout_text}") from exc

    # C 程式在解析失敗時會輸出 {"error":"..."}
    if isinstance(result, dict) and "error" in result:
        raise ValueError(result.get("error", "unknown_error"))

    return result


@app.route("/")
def index():
    """首頁：渲染 templates/index.html（Flask 預設從 templates/ 讀取）。"""
    return render_template("index.html")


@app.route("/api/level", methods=["GET"])
def api_level():
    """回傳目前關卡佈局與剩餘鳥/豬數量，供前端 Canvas 繪製。"""
    if not _game_session:
        reset_game_session()

    return jsonify(
        {
            **DEFAULT_LEVEL,
            "remaining_birds": _game_session.get("remaining_birds", LEVEL_BIRD_QUEUE_CAPACITY),
            "remaining_pigs": _game_session.get("remaining_pigs", 1),
            "total_birds": _game_session.get("total_birds", LEVEL_BIRD_QUEUE_CAPACITY),
            "obstacles": _game_session.get("obstacles", DEFAULT_LEVEL["obstacles"]),
            "birds": _game_session.get("birds", []),
        }
    )


@app.route("/api/reset", methods=["POST", "OPTIONS"])
def api_reset():
    """
    POST /api/reset
    重新初始化 C 關卡：配置 Obstacle 陣列、3 鳥 FIFO Queue，並釋放上一輪 subprocess 記憶體。
    """
    if request.method == "OPTIONS":
        return "", 204

    try:
        result = reset_game_session()
    except (FileNotFoundError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        logger.exception("reset 失敗")
        return jsonify({"error": str(exc)}), 500

    return jsonify(
        {
            **result,
            "remaining_birds": _game_session["remaining_birds"],
            "remaining_pigs": _game_session["remaining_pigs"],
            "total_birds": _game_session["total_birds"],
            "obstacles": _game_session["obstacles"],
            "birds": _game_session["birds"],
        }
    )


@app.route("/api/launch", methods=["POST", "OPTIONS"])
def api_launch():
    """
    POST /api/launch
    請求 JSON 範例：
        {"angle": 45, "velocity": 100}

    可選欄位（會一併轉給 C，未提供則由 C 端使用預設值）：
        start_x, start_y, gravity, time_step, max_steps,
        min_x, min_y, max_x, max_y, obstacles
    """
    if request.method == "OPTIONS":
        return "", 204

    body = request.get_json(silent=True)
    if not body:
        return jsonify({"error": "請提供 JSON 請求體，例如 {\"angle\":45,\"velocity\":100}"}), 400

    if "angle" not in body or "velocity" not in body:
        return jsonify({"error": "缺少必要欄位 angle 或 velocity"}), 400

    try:
        angle = float(body["angle"])
        velocity = float(body["velocity"])
    except (TypeError, ValueError):
        return jsonify({"error": "angle 與 velocity 必須為數字"}), 400

    if not _game_session:
        reset_game_session()

    if _game_session.get("remaining_birds", 0) <= 0:
        return jsonify({"error": "沒有剩餘小鳥可發射"}), 400

    # 傳給 C：launch 從 game_state.json 載入持久化障礙物（含已摧毀木箱）
    c_payload: dict[str, Any] = {
        "action": "launch",
        "angle": angle,
        "velocity": velocity,
        "gravity": DEFAULT_LEVEL["gravity"],
        "time_step": DEFAULT_LEVEL["time_step"],
        **_default_bounds(),
    }

    optional_keys = (
        "start_x",
        "start_y",
        "bird_radius",
        "gravity",
        "time_step",
        "max_steps",
    )
    for key in optional_keys:
        if key in body:
            c_payload[key] = body[key]

    try:
        trajectory_data = run_physics_engine(c_payload)
    except FileNotFoundError as exc:
        return jsonify({"error": str(exc)}), 500
    except ValueError as exc:
        return jsonify({"error": f"C 引擎回報：{exc}"}), 400
    except subprocess.TimeoutExpired:
        return jsonify({"error": "物理引擎執行逾時"}), 504
    except (RuntimeError, subprocess.SubprocessError) as exc:
        logger.exception("subprocess 失敗")
        return jsonify({"error": str(exc)}), 500

    # 同步 Flask 工作階段（剩餘鳥/豬、障礙物存活狀態）
    _game_session["remaining_birds"] = trajectory_data.get(
        "remaining_birds", _game_session.get("remaining_birds", 0)
    )
    _game_session["remaining_pigs"] = trajectory_data.get(
        "remaining_pigs", _game_session.get("remaining_pigs", 0)
    )
    if "obstacles" in trajectory_data:
        _game_session["obstacles"] = trajectory_data["obstacles"]
    if "birds" in trajectory_data:
        _game_session["birds"] = trajectory_data["birds"]
    if "game_status" in trajectory_data:
        _game_session["game_status"] = trajectory_data["game_status"]

    return jsonify(trajectory_data)


@app.route("/health", methods=["GET"])
def health():
    """確認 Flask 與 C 執行檔是否存在。"""
    return jsonify(
        {
            "status": "ok",
            "sim_executable": str(SIM_EXECUTABLE),
            "sim_exists": SIM_EXECUTABLE.is_file(),
        }
    )


if __name__ == "__main__":
    # 開發模式：python app.py
    # 將 port 改為 5001，彻底避開 5000 埠的衝突與舊迷宮
    app.run(host="127.0.0.1", port=5001, debug=True)