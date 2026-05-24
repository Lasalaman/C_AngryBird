/**
 * 遊戲主邏輯：API、佇列視覺遞補、飛行狀態（橡皮筋開關）、勝負 Modal
 */

(function () {
  "use strict";

  const R = window.GameRenderer;
  const TOTAL_BIRDS = 3;
  const MS_PER_POINT = 20;

  /** 發射錨點預設：Y 字兩撇中間上方空洞（較舊交叉點更高） */
  const DEFAULT_ANCHOR = { x: 120, y: 348 };

  const canvas = document.getElementById("simCanvas");
  const ctx = canvas.getContext("2d");
  const launchBtn = document.getElementById("launchBtn");
  const resetBtn = document.getElementById("resetBtn");
  const birdDotsEl = document.getElementById("birdDots");
  const btnSpinner = launchBtn.querySelector(".btn-spinner");
  const statusBadge = document.getElementById("statusBadge");
  const toastEl = document.getElementById("toast");
  const resultMeta = document.getElementById("resultMeta");
  const metaHit = document.getElementById("metaHit");
  const metaOob = document.getElementById("metaOob");
  const metaBirds = document.getElementById("metaBirds");
  const gameModal = document.getElementById("gameModal");
  const modalTitle = document.getElementById("modalTitle");
  const modalMessage = document.getElementById("modalMessage");
  const modalIcon = document.getElementById("modalIcon");
  const modalResetBtn = document.getElementById("modalResetBtn");

  let toastTimer = null;

  /**
   * gamePhase 控制橡皮筋與排隊繪製
   * - ready：拉弓狀態，顯示繩子 + 排隊小鳥
   * - computing：等待 C 引擎（仍顯示 ready 姿態，繩子可保留至起飛）
   * - flying：飛行中，不畫繩子、不畫排隊，只畫飛行中鳥與彈道
   */
  const Game = {
    totalBirds: TOTAL_BIRDS,
    remainingBirds: TOTAL_BIRDS,
    remainingPigs: 1,
    gameStatus: "playing",
    gamePhase: "ready",
    canLaunch: true,
    isAnimating: false,
  };

  const Level = { obstacles: [] };

  function $(id) {
    return document.getElementById(id);
  }

  function readNumber(id, fallback) {
    const v = parseFloat($(id).value);
    return Number.isFinite(v) ? v : fallback;
  }

  function getAnchor() {
    return {
      x: readNumber("start_x", DEFAULT_ANCHOR.x),
      y: readNumber("start_y", DEFAULT_ANCHOR.y),
    };
  }

  function isFlyingPhase() {
    return Game.gamePhase === "flying";
  }

  function enrichObstacle(raw) {
    const alive = raw.is_alive !== false;
    return {
      id: raw.id,
      kind: raw.kind,
      x: raw.x,
      y: raw.y,
      width: raw.width,
      height: raw.height,
      alive,
      dying: false,
      fade: alive ? 1 : 0,
    };
  }

  function syncObstaclesFromServer(list) {
    Level.obstacles = (list || []).map(enrichObstacle);
  }

  function applySession(data) {
    syncObstaclesFromServer(data.obstacles);
    if (data.spawn) {
      if (data.spawn.start_x != null) $("start_x").value = data.spawn.start_x;
      if (data.spawn.start_y != null) $("start_y").value = data.spawn.start_y;
    } else {
      $("start_x").value = DEFAULT_ANCHOR.x;
      $("start_y").value = DEFAULT_ANCHOR.y;
    }
    Game.totalBirds = data.total_birds ?? TOTAL_BIRDS;
    Game.remainingBirds = data.remaining_birds ?? Game.totalBirds;
    Game.remainingPigs = data.remaining_pigs ?? 1;
    Game.gameStatus = data.game_status ?? "playing";
    Game.gamePhase = "ready";
    updateBirdDots();
    updateLaunchButton();
  }

  function updateBirdDots() {
    birdDotsEl.innerHTML = "";
    const used = Game.totalBirds - Game.remainingBirds;
    for (let i = 0; i < Game.totalBirds; i++) {
      const dot = document.createElement("span");
      dot.className = "bird-dot" + (i < used ? " used" : "");
      birdDotsEl.appendChild(dot);
    }
  }

  function updateLaunchButton() {
    const ok =
      Game.gameStatus === "playing" &&
      Game.canLaunch &&
      Game.gamePhase !== "flying" &&
      !Game.isAnimating &&
      Game.remainingBirds > 0;
    launchBtn.disabled = !ok;
    resetBtn.disabled = Game.isAnimating || Game.gamePhase === "flying";
  }

  function renderFrame(birdPos, trail) {
    const anchor = getAnchor();
    const ready = Game.gamePhase === "ready" || Game.gamePhase === "computing";

    R.drawFrame(ctx, canvas, {
      obstacles: Level.obstacles,
      anchorX: anchor.x,
      anchorY: anchor.y,
      remainingBirds: Game.remainingBirds,
      isFlying: isFlyingPhase(),
      showRubberBands: ready && Game.remainingBirds > 0,
      birdPos: isFlyingPhase() ? birdPos : null,
      trail: trail || [],
    });
  }

  function setBadge(state, text) {
    statusBadge.className = "badge badge-" + state;
    statusBadge.textContent = text;
  }

  function showToast(msg, type) {
    if (toastTimer) clearTimeout(toastTimer);
    toastEl.textContent = msg;
    toastEl.className = "toast show" + (type === "warn" ? " toast-warn" : "");
    toastEl.hidden = false;
    toastTimer = setTimeout(() => {
      toastEl.hidden = true;
    }, 4500);
  }

  function showModal(kind) {
    gameModal.hidden = false;
    gameModal.setAttribute("aria-hidden", "false");
    if (kind === "won") {
      modalIcon.textContent = "🏆";
      modalTitle.textContent = "遊戲勝利";
      modalMessage.textContent = "太棒了！你擊敗了綠色小豬！";
      modalTitle.className = "modal-title modal-win";
    } else {
      modalIcon.textContent = "😢";
      modalTitle.textContent = "遊戲失敗";
      modalMessage.textContent = "三隻鳥都用完了，小豬還活著。再試一次！";
      modalTitle.className = "modal-title modal-lose";
    }
    Game.canLaunch = false;
    Game.gamePhase = "ready";
    updateLaunchButton();
  }

  function hideModal() {
    gameModal.hidden = true;
    gameModal.setAttribute("aria-hidden", "true");
  }

  function circleHitsRect(cx, cy, r, obs) {
    const closestX = Math.max(obs.x, Math.min(cx, obs.x + obs.width));
    const closestY = Math.max(obs.y, Math.min(cy, obs.y + obs.height));
    const dx = cx - closestX;
    const dy = cy - closestY;
    return dx * dx + dy * dy <= r * r;
  }

  function findCollisionAt(x, y) {
    for (const obs of Level.obstacles) {
      if (!obs.alive || obs.dying) continue;
      if (circleHitsRect(x, y, R.BIRD_RADIUS, obs)) return obs;
    }
    return null;
  }

  function startObstacleFade(obs) {
    obs.dying = true;
    if (obs.kind === R.KIND_PIG) Game.remainingPigs = 0;
  }

  function tickObstacleFades() {
    let any = false;
    for (const obs of Level.obstacles) {
      if (obs.dying) {
        any = true;
        obs.fade = Math.max(0, obs.fade - 0.1);
        if (obs.fade <= 0) {
          obs.alive = false;
          obs.dying = false;
        }
      }
    }
    return any;
  }

  const Animator = { frameId: null, trail: [], running: false };

  function cancelAnimation() {
    if (Animator.frameId != null) cancelAnimationFrame(Animator.frameId);
    Animator.running = false;
    Game.isAnimating = false;
    Animator.trail = [];
  }

  function playFlightAnimation(trajectory, simResult) {
    cancelAnimation();
    Animator.running = true;
    Game.isAnimating = true;
    Game.gamePhase = "flying";
    updateLaunchButton();

    let index = 0;
    let lastTime = performance.now();
    let hitDetected = false;
    let frozenBird = trajectory[0];

    return new Promise((resolve) => {
      function finish() {
        Animator.running = false;
        Game.isAnimating = false;
        Game.gamePhase = "ready";
        resolve({ hit: hitDetected || simResult.hit });
      }

      function step(now) {
        if (!Animator.running) {
          finish();
          return;
        }

        if (now - lastTime >= MS_PER_POINT && !hitDetected && index < trajectory.length) {
          lastTime = now;
          const p = trajectory[index];
          Animator.trail.push({ x: p.x, y: p.y });
          frozenBird = p;
          const hit = findCollisionAt(p.x, p.y);
          if (hit) {
            hitDetected = true;
            startObstacleFade(hit);
          } else {
            index++;
          }
        }

        renderFrame({ x: frozenBird.x, y: frozenBird.y }, Animator.trail);
        const fading = tickObstacleFades();
        const atEnd = index >= trajectory.length;

        if ((hitDetected || atEnd) && !fading) {
          if (!hitDetected && simResult.hit && simResult.hit_id) {
            const t = Level.obstacles.find((o) => o.id === simResult.hit_id);
            if (t?.alive) startObstacleFade(t);
            hitDetected = true;
          }
          finish();
          return;
        }
        Animator.frameId = requestAnimationFrame(step);
      }
      Animator.frameId = requestAnimationFrame(step);
    });
  }

  async function apiReset() {
    const res = await fetch("/api/reset", { method: "POST" });
    const data = await res.json();
    if (!res.ok) throw new Error(data?.error || "重置失敗");
    return data;
  }

  async function resetLevel() {
    if (Game.isAnimating) return;
    hideModal();
    resetBtn.disabled = true;
    setBadge("loading", "重置中");
    try {
      const data = await apiReset();
      applySession(data);
      Animator.trail = [];
      Game.canLaunch = true;
      Game.gameStatus = "playing";
      Game.gamePhase = "ready";
      renderFrame(null, []);
      setBadge("idle", "就緒");
      showToast("關卡已重製：3 隻鳥、障礙物已恢復", "warn");
    } catch (e) {
      showToast(e.message);
      setBadge("error", "失敗");
    } finally {
      resetBtn.disabled = false;
      updateLaunchButton();
    }
  }

  async function initGame() {
    try {
      const res = await fetch("/api/level");
      const data = await res.json();
      if (!res.ok) throw new Error();
      applySession(data);
      if (data.spawn?.start_y === 380) {
        $("start_y").value = DEFAULT_ANCHOR.y;
      }
    } catch {
      await resetLevel();
      return;
    }
    renderFrame(null, []);
    setBadge("idle", "就緒");
  }

  async function launch() {
    if (!Game.canLaunch || Game.remainingBirds <= 0) return;
    if (Game.gameStatus !== "playing") return;
    if (Game.gamePhase === "flying") return;

    cancelAnimation();
    Game.gamePhase = "computing";
    launchBtn.disabled = true;
    btnSpinner.hidden = false;
    setBadge("loading", "計算中");
    renderFrame(null, []);

    const anchor = getAnchor();
    const payload = {
      angle: readNumber("angle", 35),
      velocity: readNumber("velocity", 550),
      gravity: readNumber("gravity", 980),
      start_x: anchor.x,
      start_y: anchor.y,
    };

    try {
      const res = await fetch("/api/launch", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data?.error || "發射失敗");
      if (!data.trajectory?.length) throw new Error("無軌跡資料");

      btnSpinner.hidden = true;
      setBadge("loading", "飛行中");

      const anim = await playFlightAnimation(data.trajectory, data);

      if (data.obstacles) syncObstaclesFromServer(data.obstacles);

      Game.remainingBirds = data.remaining_birds ?? Game.remainingBirds;
      Game.remainingPigs = data.remaining_pigs ?? Game.remainingPigs;
      Game.gameStatus = data.game_status ?? Game.gameStatus;
      Game.gamePhase = "ready";

      resultMeta.hidden = false;
      metaHit.textContent = anim.hit ? "是！擊中目標！" : "否";
      metaHit.className = anim.hit ? "meta-hit-success" : "";
      metaOob.textContent = data.out_of_bounds ? "是" : "否";
      metaBirds.textContent = String(Game.remainingBirds);
      updateBirdDots();

      if (Game.gameStatus === "won") {
        showModal("won");
        setBadge("ok", "勝利");
      } else if (Game.gameStatus === "lost") {
        showModal("lost");
        setBadge("error", "失敗");
      } else {
        Game.canLaunch = true;
        Animator.trail = [];
        renderFrame(null, []);
        setBadge("idle", `下一隻 · 剩 ${Game.remainingBirds} 隻`);
      }
    } catch (e) {
      showToast(e.message || "發射失敗");
      setBadge("error", "失敗");
      Game.gamePhase = "ready";
      renderFrame(null, []);
      Game.canLaunch = true;
    } finally {
      btnSpinner.hidden = true;
      Game.isAnimating = false;
      updateLaunchButton();
    }
  }

  launchBtn.addEventListener("click", launch);
  resetBtn.addEventListener("click", resetLevel);
  modalResetBtn.addEventListener("click", resetLevel);

  $("start_x").value = DEFAULT_ANCHOR.x;
  $("start_y").value = DEFAULT_ANCHOR.y;

  initGame();
})();
