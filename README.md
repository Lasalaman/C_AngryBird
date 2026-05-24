# C 語言核心 × Flask 網頁前後端分層架構——憤怒鳥物理引擎遊戲

**開發者：** [Lasalaman](https://github.com/Lasalaman)

本專案是一款結合 **C 語言底層運算** 與 **現代網頁分層架構** 的憤怒鳥（Angry Bird）風格物理射擊遊戲。玩家透過瀏覽器調整發射角度與初速，由手寫 C 物理核心計算拋物線軌跡與碰撞結果，Flask 後端以子行程（subprocess）安全橋接前後端，HTML5 Canvas 負責精美視覺與互動動畫。

本專案**摒棄 Unity、Godot 等現成遊戲引擎**，從零實作：

- 拋體運動公式與邊界檢查
- `struct` 封裝的遊戲實體（小鳥、障礙物、小豬）
- `malloc` / `free` 動態配置關卡障礙物陣列
- **FIFO 佇列（Queue）** 管理待發射小鳥
- 跨請求的關卡狀態持久化（`game_state.json`）

適合展示系統程式、資料結構、記憶體管理與前後端分層整合的完整實作流程。

---

## 技術特點 (Technical Highlights)

### 【C 語言底層掌控】

遊戲世界中的每一類實體皆以 `struct` 明確封裝，並在標頭檔中以繁體中文註解標示記憶體布局與欄位語意：

| 結構體 | 檔案 | 說明 |
|--------|------|------|
| `Bird` | `include/bird.h` | 座標、速度、半徑、品種、存活與發射狀態 |
| `Obstacle` | `include/obstacle.h` | 豬／木箱共用結構，含 AABB 碰撞盒與 `hit_points` |
| `Level` / `LevelState` | `include/level.h` | 關卡邊界、重力、剩餘鳥／豬計數 |
| `BirdQueue` | `include/bird_queue.h` | 環形緩衝區 FIFO 佇列 |
| `GameSession` | `include/bird_game.h` | 跨 subprocess 的關卡持久化快照 |

**動態記憶體配置：** 每關障礙物數量可不同。`init_level()` 依 `obstacle_count` 計算 `malloc(obstacle_count * sizeof(Obstacle))`，配置連續陣列；關卡結束時由 `free_level()` 一次 `free` 整塊記憶體，避免對每個元素個別配置造成管理複雜度。

```c
/* 大小計算範例：5 個障礙物 × sizeof(Obstacle) ≈ 320 bytes */
level->obstacles = (Obstacle *)malloc(obstacle_count * sizeof(Obstacle));
```

---

### 【進階資料結構應用：Queue 佇列】

彈弓後方待發射小鳥以 **FIFO 佇列** 管理（`src/bird_queue.c`）：

- `bird_queue_create(capacity)`：`malloc` 控制結構 + `malloc(capacity * sizeof(Bird))` 環形緩衝區
- `bird_queue_enqueue`：寫入佇列尾端（O(1)）
- `bird_queue_dequeue`：取出佇列前端，供發射消耗（O(1)）
- 每關預設 **3 隻鳥**（`LEVEL_BIRD_QUEUE_CAPACITY`）

發射時 C 核心呼叫 `level_consume_bird_from_queue()` **Dequeue 一隻鳥**，更新 `remaining_birds`，不對單一 `Bird` 額外 `malloc`／`free`，避免雙重釋放。子行程結束時由 `bird_queue_destroy()` 統一釋放 `buffer` 與佇列本體。

---

### 【狀態持久化 (State Persistence)】

因 Flask 每次發射啟動**新的 C 子行程**，堆積記憶體無法跨請求保留。本專案以 `src/bird_game.c` 實作 **`GameSession`** 與檔案 **`game_state.json`**：

1. **Launch**：從檔案載入關卡 → 模擬拋體 → 若碰撞則 `obstacle_apply_damage()` 將目標 `is_alive` 設為 `false` → 寫回 JSON
2. **再次 Launch**：已摧毀的木箱 **`is_alive: false`** 不會參與碰撞、不會被繪製，**不會死而復生**
3. **RESET**：刪除舊狀態，重新載入預設關卡（3 鳥、全障礙物存活）

此設計確保 C 核心「確實修改」障礙物存活狀態，而非每次讀取乾淨的關卡模板。

---

### 【安全行程通訊：Flask × subprocess】

Flask（`app.py`）透過 `subprocess.run` 呼叫 `angrybird_sim.exe`，以 **stdin／stdout 交換 JSON**：

| 安全措施 | 說明 |
|----------|------|
| 不使用 `shell=True` | 直接執行可執行檔路徑，降低命令注入風險 |
| `timeout=10` | 避免 C 程式異常阻塞 Flask 主行程 |
| `capture_output=True` | 子行程 stdout 不與 Flask 日誌混流 |
| `text=True` | UTF-8 字串傳遞，便於 `json.loads` |
| `input=` 管道寫入 | 寫入後關閉 stdin，C 端 `fgets` 讀取一行 JSON |

**請求範例（發射）：**

```json
{"action":"launch","angle":35,"velocity":550}
```

**回應範例：**

```json
{
  "trajectory": [{"t":0,"x":120,"y":380}, "..."],
  "hit": true,
  "remaining_birds": 2,
  "remaining_pigs": 0,
  "game_status": "won",
  "obstacles": [{"id":2,"is_alive":false, "...": "..."}]
}
```

---

## 系統架構 (Architecture)

```
┌─────────────────────────────────────────────────────────────────┐
│  網頁前端 (Browser)                                              │
│  templates/index.html · static/js/renderer.js · simulator.js    │
│  HTML5 Canvas 繪圖 · Fetch API · 飛行動畫 · 勝負 Modal         │
└───────────────────────────┬─────────────────────────────────────┘
                            │ HTTP JSON
                            │  GET  /api/level
                            │  POST /api/launch
                            │  POST /api/reset
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  Flask 後端 (app.py)                                             │
│  路由 · 工作階段 (_game_session) · subprocess 編排               │
└───────────────────────────┬─────────────────────────────────────┘
                            │ subprocess.run([angrybird_sim.exe])
                            │ stdin  → JSON 指令
                            │ stdout ← JSON 軌跡與關卡狀態
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  C 語言物理引擎 (angrybird_sim.exe)                              │
│  main.c · physics.c · level.c · bird_queue.c · bird_game.c      │
│  拋體運動 · 碰撞偵測 · Queue Dequeue · game_state.json 持久化   │
└─────────────────────────────────────────────────────────────────┘
```

**資料流（單次發射）：**

```
使用者按「發射」
  → JS POST /api/launch { angle, velocity }
  → Flask 組裝 JSON，subprocess 呼叫 C
  → C：載入 game_state.json → Dequeue 一鳥 → 計算軌跡 → 更新障礙物 → 寫回檔案
  → stdout 回傳 trajectory + remaining_birds + game_status
  → Flask jsonify 回傳前端
  → Canvas requestAnimationFrame 沿軌跡播放動畫
```

---

## 執行方式 (How to Run)

### 環境需求

| 項目 | 版本建議 |
|------|----------|
| **GCC** | 支援 C11（`gcc` 或 `MinGW-w64`） |
| **Make** | GNU Make（Windows 可選裝或使用手動 `gcc` 編譯） |
| **Python** | 3.10+ |
| **瀏覽器** | Chrome / Edge / Firefox（支援 Canvas 與 Fetch） |

### 1. 取得專案

```bash
git clone https://github.com/Lasalaman/C_AngryBird.git
cd C_AngryBird
```

### 2. 編譯 C 核心

**使用 Make（Linux / macOS / Git Bash）：**

```bash
make
```

產出：

- `angrybird_sim`（或 Windows 下 `angrybird_sim.exe`）
- `libangrybird.a`（靜態函式庫，可選）

**Windows 手動編譯（PowerShell）：**

```powershell
gcc -Wall -Wextra -std=c11 -Iinclude -o angrybird_sim.exe `
  src/main.c src/level.c src/level_defaults.c src/bird_game.c `
  src/obstacle.c src/bird.c src/bird_queue.c src/physics.c src/json_io.c -lm
```

### 3. 安裝 Python 依賴

```bash
pip install -r requirements.txt
```

或：

```bash
pip install Flask
```

### 4. 啟動 Flask 伺服器

```bash
python app.py
```

預設監聽：

```text
http://127.0.0.1:5001/
```

> 註：`app.py` 使用 **5001** 埠，以避免與本機其他服務佔用 5000 衝突。若你自行改回 5000，請以實際設定為準。

### 5. 健康檢查（可選）

```bash
curl http://127.0.0.1:5001/health
```

確認 `sim_exists: true` 表示 C 執行檔路徑正確。

### 清理編譯產物

```bash
make clean
```

---

## 遊戲規則與視覺呈現

### 視覺呈現（HTML5 Canvas）

前端由 `static/js/renderer.js` 負責繪製：

| 元素 | 說明 |
|------|------|
| **背景** | 藍色天空漸層、白色雲朵 |
| **前景** | 綠色草皮、裝飾用樹木（無碰撞） |
| **彈弓** | Y 字形木架，橡皮筋連接待發射小鳥 |
| **小鳥** | 橘色圓形角色，沿螢光綠**虛線彈道**飛行（`requestAnimationFrame`） |
| **障礙物** | 擬真木紋木箱（可擊碎並持久化消失） |
| **目標** | 圓滾滾的綠色小豬 |

頂部以 **3 顆紅點** 顯示剩餘小鳥數量；用過的鳥會變灰。

### 遊戲規則

| 規則 | 說明 |
|------|------|
| **發射次數** | 每關共 **3 隻鳥**（FIFO 佇列） |
| **勝利條件** | 任一發射擊中並摧毀 **綠色小豬** → 畫面中央顯示 **「遊戲勝利」** 彈窗 |
| **失敗條件** | **3 發全部用完**且小豬仍存活 → 顯示 **「遊戲失敗」** 彈窗 |
| **關卡重製** | 點擊彈窗或頂部 **「重置關卡」** → `POST /api/reset` → C 重新配置記憶體與 Queue，恢復 3 鳥與所有障礙物 |
| **木箱狀態** | 已擊碎的木箱在後續發射中**維持消失**，直至 RESET |

### 操作方式

1. 調整左側 **角度（Angle）** 與 **初速（Velocity）**
2. 點擊 **「發射 Launch」**
3. 觀看小鳥沿拋物線飛行與碰撞效果
4. 若仍有剩餘小鳥且未通關，可調整參數進行下一發

---

## 專案目錄結構

```text
C_AngryBird/
├── app.py                 # Flask 後端入口
├── requirements.txt       # Python 依賴
├── Makefile               # 編譯 C 核心與模擬器
├── game_state.json        # 關卡持久化（執行後自動產生）
├── angrybird_sim.exe      # C 物理引擎（編譯產物）
├── include/               # C 標頭檔
│   ├── common.h
│   ├── bird.h · bird_queue.h
│   ├── obstacle.h · level.h
│   ├── physics.h · json_io.h
│   ├── bird_game.h · level_defaults.h
├── src/                   # C 原始碼
│   ├── main.c             # subprocess 入口（reset / launch）
│   ├── physics.c · level.c · bird_queue.c
│   ├── bird_game.c        # 狀態持久化
│   └── ...
├── templates/
│   └── index.html         # 遊戲首頁
└── static/
    ├── css/style.css
    └── js/
        ├── renderer.js    # Canvas 精美繪圖
        └── simulator.js   # 遊戲邏輯與 API
```

---

## API 端點摘要

| 方法 | 路徑 | 說明 |
|------|------|------|
| `GET` | `/` | 遊戲首頁 |
| `GET` | `/api/level` | 取得關卡佈局與剩餘鳥／豬 |
| `POST` | `/api/launch` | 發射一鳥，回傳軌跡與更新後狀態 |
| `POST` | `/api/reset` | 重置關卡（3 鳥、全障礙物復活） |
| `GET` | `/health` | 檢查 Flask 與 C 執行檔狀態 |

---

## 附錄：AI 協作與 Prompt 軌跡 (Appendix)

本專案採 **Co-pilot 精神**，由開發者（Lasalaman）以分階段、可驗證的 Prompt 引導 AI 協作完成，而非一次性產出整包程式。以下為實際迭代軌跡摘要：

| 階段 | 任務焦點 | 產出與學習重點 |
|------|----------|----------------|
| **任務 1** | C `struct` 設計 | `Bird`、`Obstacle`、`LevelState`、`BirdQueue` 標頭與記憶體布局註解 |
| **任務 2** | `init_level` / `free_level` | `malloc` 障礙物陣列、成對釋放、避免 Memory Leak |
| **任務 3** | 拋體運動 + JSON I/O | `physics.c`、`main.c` stdin／stdout、`angrybird_sim.exe` |
| **任務 4** | Flask subprocess | `app.py`、`/api/launch`、安全 `subprocess.run` 說明 |
| **任務 5** | 網頁前端 | `templates/`、`static/`、Canvas 軌跡繪製 |
| **任務 6** | 互動關卡 | 豬與木箱渲染、`requestAnimationFrame` 飛行動畫 |
| **任務 7** | 多鳥 Queue | 3 鳥 FIFO、連續發射、重置關卡 |
| **任務 8** | 狀態持久化與視覺 | `bird_game.c`、`game_state.json`、勝負 Modal、木箱不再復活 |
| **任務 9** | 文件化 | 本 `README.md` |

**協作原則：**

1. **先結構、後實作**：每階段只要求標頭與函式原型，再補 `.c` 實作，降低一次改壞全專案的風險。
2. **可執行驗證**：C 端以 `echo '{...}' | ./angrybird_sim.exe` 驗證 JSON；Flask 以 `curl`／瀏覽器驗證 API。
3. **明確拒絕項**：例如「不要寫 main」「不要用 shell=True」「發射時不要重置已摧毀木箱」。
4. **對齊 JSON 契約**：前後端與 C 三方欄位名稱（`angle`、`velocity`、`remaining_birds`、`is_alive`）保持一致。
5. **記憶體與安全註解**：要求繁體中文註解說明 `malloc` 大小計算、指標生命週期與 Queue 釋放順序。

此流程示範如何在課程或專題中，將 AI 作為**結對程式設計夥伴**，而非取代思考的黑箱生成器。

---

## 授權與聯絡

本專案由 [Lasalaman](https://github.com/Lasalaman) 開發，供學習與展示用途。

若發現問題或希望貢獻改進，歡迎於 GitHub 提出 Issue 或 Pull Request。
