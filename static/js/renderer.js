/**
 * Canvas 精美渲染：天空、彈弓、排隊小鳥、草皮、障礙物、彈道
 */
(function () {
  "use strict";

  const KIND_PIG = 1;
  const KIND_BOX = 2;
  const BIRD_RADIUS = 16;

  /** 彈弓幾何常數（anchor 為發射錨點＝小鳥靜止中心） */
  const SLING = {
    FORK_SPREAD: 22,
    FORK_RISE: 42,
    POLE_LENGTH: 38,
    QUEUE_GAP: 30,
    QUEUE_LEFT: 58,
    QUEUE_DOWN: 36,
  };

  const clouds = [
    { x: 180, y: 90, s: 1.0 },
    { x: 520, y: 60, s: 0.85 },
    { x: 900, y: 110, s: 1.1 },
    { x: 1150, y: 70, s: 0.75 },
  ];

  const decorTrees = [
    { x: 200, y: 620 },
    { x: 1100, y: 610 },
    { x: 1240, y: 630 },
  ];

  function drawCloud(ctx, x, y, scale) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(scale, scale);
    ctx.fillStyle = "rgba(255,255,255,0.92)";
    ctx.beginPath();
    ctx.arc(0, 0, 28, 0, Math.PI * 2);
    ctx.arc(32, -6, 34, 0, Math.PI * 2);
    ctx.arc(68, 0, 26, 0, Math.PI * 2);
    ctx.arc(28, 10, 22, 0, Math.PI * 2);
    ctx.fill();
    ctx.restore();
  }

  function drawDecorTree(ctx, x, baseY) {
    ctx.fillStyle = "#4a3728";
    ctx.fillRect(x - 8, baseY - 70, 16, 70);
    ctx.fillStyle = "#2d6a4f";
    ctx.beginPath();
    ctx.arc(x, baseY - 95, 42, 0, Math.PI * 2);
    ctx.arc(x - 30, baseY - 75, 32, 0, Math.PI * 2);
    ctx.arc(x + 30, baseY - 75, 32, 0, Math.PI * 2);
    ctx.fill();
  }

  /**
   * 由發射錨點（空洞處）推算彈弓 Y 字架與橡皮筋端點
   * @param {number} anchorX 發射起點 x（與物理引擎 start_x 一致）
   * @param {number} anchorY 發射起點 y（Y 字兩撇中間上方空洞，較舊版交叉點更高）
   */
  function getSlingshotGeometry(anchorX, anchorY) {
    const junctionY = anchorY + 10;
    return {
      anchor: { x: anchorX, y: anchorY },
      forkLeft: { x: anchorX - SLING.FORK_SPREAD, y: junctionY - SLING.FORK_RISE },
      forkRight: { x: anchorX + SLING.FORK_SPREAD, y: junctionY - SLING.FORK_RISE },
      junction: { x: anchorX, y: junctionY },
      poleBottom: { x: anchorX, y: anchorY + SLING.POLE_LENGTH },
    };
  }

  /**
   * 排隊槽位座標（slot 0 = 發射點，1、2 = 左下方水平等候）
   * @param {object} geom getSlingshotGeometry 回傳值
   * @param {number} slotIndex 0 為即將發射，1、2 為後方排隊
   */
  function getQueueSlotPosition(geom, slotIndex) {
    if (slotIndex === 0) {
      return { x: geom.anchor.x, y: geom.anchor.y };
    }
    const row = slotIndex - 1;
    return {
      x: geom.anchor.x - SLING.QUEUE_LEFT - row * SLING.QUEUE_GAP,
      y: geom.anchor.y + SLING.QUEUE_DOWN,
    };
  }

  window.GameRenderer = {
    KIND_PIG,
    KIND_BOX,
    BIRD_RADIUS,
    SLING,
    getSlingshotGeometry,
    getQueueSlotPosition,

    drawBackground(ctx, w, h) {
      const sky = ctx.createLinearGradient(0, 0, 0, h);
      sky.addColorStop(0, "#4facfe");
      sky.addColorStop(0.55, "#87ceeb");
      sky.addColorStop(1, "#b8e0f0");
      ctx.fillStyle = sky;
      ctx.fillRect(0, 0, w, h);

      for (const c of clouds) {
        drawCloud(ctx, c.x, c.y, c.s);
      }

      const grassH = 56;
      const grassY = h - grassH;
      const grass = ctx.createLinearGradient(0, grassY, 0, h);
      grass.addColorStop(0, "#5cb85c");
      grass.addColorStop(1, "#3d8b3d");
      ctx.fillStyle = grass;
      ctx.fillRect(0, grassY, w, grassH);

      ctx.strokeStyle = "rgba(45,90,45,0.5)";
      for (let i = 0; i < w; i += 18) {
        ctx.beginPath();
        ctx.moveTo(i, grassY + 8);
        ctx.lineTo(i + 6, grassY);
        ctx.stroke();
      }

      for (const t of decorTrees) {
        drawDecorTree(ctx, t.x, h - 8);
      }
    },

    /**
     * 繪製 Y 字彈弓架
     * @param {boolean} showRubberBands 僅準備發射時 true；飛行中不畫繩子
     * @param {{x:number,y:number}|null} bandTarget 橡皮筋連接點（通常為錨點／小鳥中心）
     */
    drawSlingshot(ctx, geom, showRubberBands, bandTarget) {
      ctx.strokeStyle = "#5c4033";
      ctx.lineWidth = 7;
      ctx.lineCap = "round";

      ctx.beginPath();
      ctx.moveTo(geom.junction.x, geom.junction.y);
      ctx.lineTo(geom.poleBottom.x, geom.poleBottom.y);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(geom.junction.x, geom.junction.y);
      ctx.lineTo(geom.forkLeft.x, geom.forkLeft.y);
      ctx.moveTo(geom.junction.x, geom.junction.y);
      ctx.lineTo(geom.forkRight.x, geom.forkRight.y);
      ctx.stroke();

      if (showRubberBands && bandTarget) {
        ctx.strokeStyle = "#2a1a0a";
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.moveTo(geom.forkLeft.x, geom.forkLeft.y);
        ctx.lineTo(bandTarget.x, bandTarget.y);
        ctx.moveTo(geom.forkRight.x, geom.forkRight.y);
        ctx.lineTo(bandTarget.x, bandTarget.y);
        ctx.stroke();
      }
    },

    drawWoodBox(ctx, obs) {
      if (!obs.alive && obs.fade <= 0) return;
      ctx.save();
      ctx.globalAlpha = obs.fade;
      const { x, y, width: w, height: h } = obs;

      ctx.fillStyle = "#a0522d";
      ctx.fillRect(x, y, w, h);
      ctx.strokeStyle = "#6b3e1e";
      ctx.lineWidth = 2;
      ctx.strokeRect(x + 1, y + 1, w - 2, h - 2);

      ctx.strokeStyle = "rgba(80,45,20,0.55)";
      ctx.lineWidth = 1;
      for (let i = 8; i < w; i += 12) {
        ctx.beginPath();
        ctx.moveTo(x + i, y);
        ctx.lineTo(x + i - 4, y + h);
        ctx.stroke();
      }
      for (let j = 6; j < h; j += 10) {
        ctx.beginPath();
        ctx.moveTo(x, y + j);
        ctx.lineTo(x + w, y + j + 3);
        ctx.stroke();
      }
      ctx.restore();
    },

    drawPig(ctx, obs) {
      if (!obs.alive && obs.fade <= 0) return;
      ctx.save();
      ctx.globalAlpha = obs.fade;
      const cx = obs.x + obs.width * 0.5;
      const cy = obs.y + obs.height * 0.5;
      const r = Math.min(obs.width, obs.height) * 0.5;

      ctx.fillStyle = "#65d36e";
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = "#2f9e44";
      ctx.lineWidth = 3;
      ctx.stroke();

      ctx.fillStyle = "#3d9a46";
      ctx.beginPath();
      ctx.ellipse(cx, cy + r * 0.42, r * 0.62, r * 0.38, 0, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = "#fff";
      ctx.beginPath();
      ctx.arc(cx - r * 0.32, cy - r * 0.1, r * 0.16, 0, Math.PI * 2);
      ctx.arc(cx + r * 0.32, cy - r * 0.1, r * 0.16, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = "#1a3d1a";
      ctx.beginPath();
      ctx.arc(cx - r * 0.3, cy - r * 0.08, r * 0.07, 0, Math.PI * 2);
      ctx.arc(cx + r * 0.3, cy - r * 0.08, r * 0.07, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = "#e8a0a0";
      ctx.beginPath();
      ctx.ellipse(cx, cy + r * 0.15, r * 0.2, r * 0.14, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    },

    drawBird(ctx, x, y, alpha) {
      ctx.save();
      ctx.globalAlpha = alpha ?? 1;
      ctx.shadowColor = "rgba(0,0,0,0.25)";
      ctx.shadowBlur = 6;
      ctx.fillStyle = "#ff5c3a";
      ctx.beginPath();
      ctx.arc(x, y, BIRD_RADIUS, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = "#fff";
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.fillStyle = "#ffeb3b";
      ctx.beginPath();
      ctx.moveTo(x + BIRD_RADIUS, y);
      ctx.lineTo(x + BIRD_RADIUS + 10, y - 4);
      ctx.lineTo(x + BIRD_RADIUS + 10, y + 4);
      ctx.fill();
      ctx.fillStyle = "#fff";
      ctx.beginPath();
      ctx.arc(x + 5, y - 5, 4, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = "#222";
      ctx.beginPath();
      ctx.arc(x + 6, y - 5, 2, 0, Math.PI * 2);
      ctx.fill();
      ctx.restore();
    },

    /**
     * 依剩餘鳥數繪製排隊小鳥（對應 Queue 視覺遞補）
     * @param {number} remainingBirds 後端 remaining_birds（最多 3）
     * @param {boolean} isFlying 飛行中不繪製排隊（已離開彈弓的鳥）
     */
    drawBirdQueue(ctx, geom, remainingBirds, isFlying) {
      if (isFlying || remainingBirds <= 0) {
        return;
      }

      /*
       * 迴圈邏輯：remainingBirds 表示「尚在佇列中、尚未發射」的隻數。
       * slot 0 → 發射錨點（下一隻將從此處擊出）
       * slot 1、2 → 左下方水平間距 SLING.QUEUE_GAP 排隊
       * 當一隻飛走後 remainingBirds 減 1，視覺上僅繪製 0..remaining-1，
       * 等同第 2、3 隻往前遞補到 slot 0、1。
       */
      for (let slot = 0; slot < remainingBirds; slot++) {
        const pos = getQueueSlotPosition(geom, slot);
        const dim = slot > 0 ? 0.92 : 1;
        this.drawBird(ctx, pos.x, pos.y, dim);
      }
    },

    drawTrail(ctx, trail) {
      if (!trail || trail.length < 2) return;
      ctx.save();
      ctx.setLineDash([8, 6]);
      ctx.beginPath();
      ctx.moveTo(trail[0].x, trail[0].y);
      for (let i = 1; i < trail.length; i++) {
        ctx.lineTo(trail[i].x, trail[i].y);
      }
      ctx.strokeStyle = "rgba(94, 234, 212, 0.85)";
      ctx.lineWidth = 3;
      ctx.shadowColor = "rgba(94, 234, 212, 0.6)";
      ctx.shadowBlur = 10;
      ctx.stroke();
      ctx.restore();
    },

    drawFrame(ctx, canvas, state) {
      const w = canvas.width;
      const h = canvas.height;
      this.drawBackground(ctx, w, h);

      for (const obs of state.obstacles) {
        if (!obs.alive && obs.fade <= 0) continue;
        if (obs.kind === KIND_PIG) this.drawPig(ctx, obs);
        else this.drawWoodBox(ctx, obs);
      }

      const anchorX = state.anchorX ?? 120;
      const anchorY = state.anchorY ?? 348;
      const geom = getSlingshotGeometry(anchorX, anchorY);

      const isFlying = state.isFlying === true;
      const showBands = state.showRubberBands === true && !isFlying;

      const bandTarget = showBands ? geom.anchor : null;
      this.drawSlingshot(ctx, geom, showBands, bandTarget);

      this.drawBirdQueue(ctx, geom, state.remainingBirds ?? 0, isFlying);

      if (state.trail?.length) {
        this.drawTrail(ctx, state.trail);
      }

      if (isFlying && state.birdPos) {
        this.drawBird(ctx, state.birdPos.x, state.birdPos.y, 1);
      }
    },
  };
})();
