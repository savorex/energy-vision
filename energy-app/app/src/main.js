function setInfoCards(total_kwh) {
  // Range-aware: totals cover the selected date range, not a fixed year
  const s = new Date(document.getElementById("dateStart").value);
  const e = new Date(document.getElementById("dateEnd").value);
  let days = 365.25;
  if (!isNaN(s) && !isNaN(e) && e >= s) {
    days = Math.max(1, (e - s) / 86400000 + 1);
  }
  const perDay = total_kwh / days;
  const perYear = perDay * 365.25;

  const val0 = Math.round(perYear);
  const val1 = Math.round(perYear / 12);
  const val2 = Math.round(perDay);
  const val3 = Math.round(perDay / 24);

  document.getElementsByClassName("info-cards")[0].children[0].getElementsByClassName("val")[0].textContent = `${val0}`;
  document.getElementsByClassName("info-cards")[0].children[1].getElementsByClassName("val")[0].textContent = `${val1}`;
  document.getElementsByClassName("info-cards")[0].children[2].getElementsByClassName("val")[0].textContent = `${val2}`;
  document.getElementsByClassName("info-cards")[0].children[3].getElementsByClassName("val")[0].textContent = `${val3}`;
}

(function () {
  var wrap = document.getElementById('sliderWrap');
  var track = document.getElementById('sliderTrack');
  var fill = document.getElementById('sliderFill');
  var hStart = document.getElementById('handleStart');
  var hEnd = document.getElementById('handleEnd');
  var ticks = document.getElementById('tickLabels');
  var dateStart = document.getElementById('dateStart');
  var dateEnd = document.getElementById('dateEnd');
  var yearSelector = document.getElementById('yearSelector');

  var currentYear = new Date().getFullYear();
  var state = {
    year: currentYear,
    start: 0,      // Jan 1 of current year
    end: dateToFrac(new Date()),   // today
  };

  function toFraction(evt) {
    var rect = track.getBoundingClientRect();
    var x = (evt.touches ? evt.touches[0].clientX : evt.clientX) - rect.left;
    return Math.min(1, Math.max(0, x / rect.width));
  }

  function render() {
    fill.style.left = (state.start * 100) + '%';
    fill.style.width = ((state.end - state.start) * 100) + '%';
    hStart.style.left = (state.start * 100) + '%';
    hEnd.style.left = (state.end * 100) + '%';
    hStart.setAttribute('aria-valuenow', Math.round(state.start * 100));
    hEnd.setAttribute('aria-valuenow', Math.round(state.end * 100));
    updateDates();
  }

  function updateDates() {
    var s = fractionToDate(state.start, state.year);
    var e = fractionToDate(state.end, state.year);
    if (s <= e) {
      dateStart.value = toISO(s);
      dateEnd.value = toISO(e);
    }
  }

  function fractionToDate(frac, year) {
    if (frac >= 1) return new Date(year, 11, 31, 23, 59, 59);
    var start = new Date(year, 0, 1).getTime();
    var end = new Date(year + 1, 0, 1).getTime();
    return new Date(start + frac * (end - start));
  }

  function toISO(d) {
    return d.getFullYear() + '-' +
      String(d.getMonth() + 1).padStart(2, '0') + '-' +
      String(d.getDate()).padStart(2, '0');
  }

  function dateToFrac(d) {
    var start = new Date(d.getFullYear(), 0, 1).getTime();
    var end = new Date(d.getFullYear() + 1, 0, 1).getTime();
    return Math.min(1, Math.max(0, (d.getTime() - start) / (end - start)));
  }

  // Month tick labels across the year
  (function buildTicks() {
    var months = ['Tammi', 'Helmi', 'Maalis', 'Huhti', 'Touko', 'Kesä',
      'Heinä', 'Elo', 'Syys', 'Loka', 'Marras', 'Joulu'];
    for (var i = 0; i < 12; i++) {
      var span = document.createElement('span');
      span.textContent = months[i];
      span.style.left = ((i + 0.5) / 12 * 100) + '%';
      ticks.appendChild(span);
    }
  })();

  // --- Dragging ---
  var activeHandle = null;

  function nearestHandle(frac) {
    var dStart = Math.abs(frac - state.start);
    var dEnd = Math.abs(frac - state.end);
    if (dStart <= dEnd) return hStart;
    return hEnd;
  }

  function maxFrac() {
    var now = new Date();
    if (state.year !== now.getFullYear()) return 1;
    return dateToFrac(now);
  }

  function applyDrag(frac) {
    var minGap = 0.01;
    var maxF = maxFrac();
    if (activeHandle === hStart) {
      state.start = Math.min(frac, state.end - minGap);
    } else {
      state.end = Math.max(frac, state.start + minGap);
    }
    if (state.end > maxF) state.end = maxF;
    render();
  }

  function onPointerDown(e) {
    var frac = toFraction(e);
    activeHandle = nearestHandle(frac);
    activeHandle.classList.add('dragging');
    applyDrag(frac);
    document.addEventListener('pointermove', onPointerMove);
    document.addEventListener('pointerup', onPointerUp);
  }

  function onPointerMove(e) {
    if (activeHandle) applyDrag(toFraction(e));
  }

  function onPointerUp() {
    if (activeHandle) activeHandle.classList.remove('dragging');
    activeHandle = null;
    document.removeEventListener('pointermove', onPointerMove);
    document.removeEventListener('pointerup', onPointerUp);
    scheduleUpdate();
  }

  wrap.addEventListener('pointerdown', onPointerDown);

  function keyStep(handle, delta) {
    if (handle === hStart) {
      state.start = Math.min(1, Math.max(0, state.start + delta));
      if (state.start > state.end - 0.01) state.start = state.end - 0.01;
    } else {
      state.end = Math.min(maxFrac(), Math.max(0, state.end + delta));
      if (state.end < state.start + 0.01) state.end = state.start + 0.01;
    }
    render();
    scheduleUpdate();
  }

  [hStart, hEnd].forEach(function (handle) {
    handle.addEventListener('keydown', function (e) {
      var step = e.shiftKey ? 1 / 12 : 1 / 96; // shift = one month
      if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') { e.preventDefault(); keyStep(handle, -step); }
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') { e.preventDefault(); keyStep(handle, step); }
    });
  });

  yearSelector.addEventListener('click', function (e) {
    var btn = e.target.closest('.year-btn');
    if (!btn) return;
    yearSelector.querySelectorAll('.year-btn').forEach(function (b) {
      b.classList.remove('active');
    });
    btn.classList.add('active');
    state.year = parseInt(btn.dataset.year, 10);
    state.start = 0;
    state.end = maxFrac();
    render();
    scrollToActiveYear();
    scheduleUpdate();
  });

  // Keep the active year visible in the scrollable selector
  function scrollToActiveYear() {
    var active = yearSelector.querySelector('.year-btn.active');
    if (active) active.scrollIntoView({ block: 'nearest', inline: 'nearest', behavior: 'smooth' });
  }

  // --- Manual date edits drive the slider back ---
  function syncFromDates() {
    var s = new Date(dateStart.value);
    var f = new Date(dateEnd.value);
    var maxF = maxFrac();
    if (!isNaN(s) && s.getFullYear() === state.year) state.start = dateToFrac(s);
    if (!isNaN(f) && f.getFullYear() === state.year) state.end = Math.min(dateToFrac(f), maxF);
    if (state.end - state.start < 0.01) state.end = Math.min(maxF, state.start + 0.01);
    render();
  }

  // Debounced refetch whenever the date range changes
  var dateDebounce = null;
  function scheduleUpdate() {
    clearTimeout(dateDebounce);
    dateDebounce = setTimeout(updateDiagrams, 250);
  }

  dateStart.addEventListener('change', syncFromDates);
  dateEnd.addEventListener('change', syncFromDates);
  dateStart.addEventListener('change', scheduleUpdate);
  dateEnd.addEventListener('change', scheduleUpdate);

  yearSelector.querySelectorAll('.year-btn').forEach(function (b) {
    if (parseInt(b.dataset.year, 10) === currentYear) b.classList.add('active');
  });

  // Initial render
  render();
  scrollToActiveYear();
})();


// SANKEY DIAGRAM //

const height = 440 + window.innerHeight * 0.1;
var width = window.innerWidth * 0.9;
const svg = d3.select("svg").attr("width", width).attr("height", height);

var selected_sankey_type = 0;

const color_default = "#424480";
const color_hover = "#9c9b76";
const color_disabled = "#777891";

function drawSankey(container, graph, offsetY, totalValue) {
  const sankey = d3.sankey()
    .nodeWidth(8)
    .nodePadding(24)
    .nodeAlign(d3.sankeyLeft)
    .extent([[48, 12], [width - 128, height - 24]]);

  const { nodes, links } = sankey({
    nodes: graph.nodes.map(d => Object.assign({}, d)),
    links: graph.links.map(d => Object.assign({}, d))
  });

  const g = container.append("g").attr("transform", `translate(20, ${offsetY})`);

  // title
  g.append("text")
    .attr("x", width / (3 * 4))
    .attr("y", height)
    .attr("text-anchor", "middle")
    .style("font-size", "18px")
    .style("font-weight", "700")
    .style("fill", "#e6e9f2")
    .text("XAMK Energy - " + graph.name);

  function pct(v) {
    return ((v / totalValue) * 100).toFixed(1) + "%";
  }

  const link = g.append("g")
    .selectAll("g")
    .data(links)
    .join("g");

  link.append("path")
    .attr("d", d3.sankeyLinkHorizontal())
    .attr("fill", "none")
    .attr("stroke", d => color_default)
    .attr("stroke-opacity", 0.7)
    .attr("stroke-width", d => Math.max(1, d.width))
    .on("mouseover", function () {
      d3.select(this)
        .attr("stroke", color_hover);
    })
    .on("mouseout", function () {
      d3.select(this)
        .attr("stroke", color_default);
    });

  link.append("text")
    .attr("x", d => (d.source.x1 + d.target.x0) / 2)
    .attr("y", d => (d.y0 + d.y1) / 2)
    .attr("dy", "0.35em")
    .attr("text-anchor", "middle")
    .attr("pointer-events", "none")
    .text(d => `${d.value.toFixed(0)} kWh (${pct(d.value)})`);

  const node = g.append("g")
    .selectAll("g")
    .data(nodes)
    .join("g");

  node.append("rect")
    .attr("x", d => d.x0)
    .attr("y", d => d.y0)
    .attr("height", d => d.y1 - d.y0)
    .attr("width", d => d.x1 - d.x0)
    .attr("fill", "#fdb92a")

  const nodeText = node.append("text")
    .attr("x", d => d.sourceLinks.length === 0 ? d.x1 + 10 : (d.x0 + d.x1) / 2)
    .attr("y", d => (d.y1 + d.y0) / 2)
    .attr("dy", "0.35em")
    .attr("font-weight", "bold")
    .attr("pointer-events", "none")
    .attr("text-anchor", d => d.sourceLinks.length === 0 ? "start" : "middle")
    .text(d => d.name);

  nodeText.each(function (d) {
    const box = this.getBBox();
    const pad = 8;
    const isEnd = d.sourceLinks.length === 0;
    const tx = isEnd ? d.x1 + 10 + box.width / 2 : (d.x0 + d.x1) / 2;
    const ty = (d.y1 + d.y0) / 2;
    d3.select(this.parentNode)
      .insert("rect", "text")
      .attr("x", tx - box.width / 2 - pad)
      .attr("y", ty - box.height / 2 - pad)
      .attr("width", box.width + pad * 2)
      .attr("height", box.height + pad * 2)
      .attr("fill", "#595ca9")
      .attr("rx", 2)
      .attr("stroke", "#000")
      .attr("stroke-width", 1);
  });
}

// ---------- AJAX data loading ----------
const API_BASE = "/xamk-energy";
const API_FULL = API_BASE + "/api/data-full";
const API_REAL = API_BASE + "/api/data-real";

function activeApi() {
  return selected_sankey_type === 0 ? API_FULL : API_REAL;
}

function buildQuery() {
  const startDate = document.getElementById("dateStart")?.value || "2024-01-01";
  const endDate = document.getElementById("dateEnd")?.value
    || new Date().toISOString().slice(0, 10);
  return `?startDate=${encodeURIComponent(startDate)}&endDate=${encodeURIComponent(endDate)}`;
}

async function loadSankeyData(signal, attempt = 0) {
  try {
    const res = await fetch(activeApi() + buildQuery(), { signal });
    if (!res.ok) {
      let msg = res.status;
      try { msg = (await res.json()).error || msg; } catch { }
      throw new Error(`API error: ${msg}`);
    }
    return res.json();
  } catch (err) {
    if (err.name === "AbortError" || attempt >= 2) throw err;
    await new Promise(r => setTimeout(r, 500 * (attempt + 1)));
    return loadSankeyData(signal, attempt + 1);
  }
}

let currentAbort = null;   // avoid overlapping renders on rapid clicks

async function drawCurrentGraph() {
  if (currentAbort) currentAbort.abort();
  currentAbort = new AbortController();

  d3.select("svg").attr("fill-opacity", 0.3);   // dim while loading

  try {
    const data = await loadSankeyData(currentAbort.signal);
    // /api/data returns Sankey-ready: { name, total_kwh, nodes: [names], links: [{source,target,value}] }
    const shaped = {
      name: data.name,
      total_kwh_mkl: data.total_kwh,
      nodes: (data.nodes || []).map(name => ({ name })),
      links: data.links || [],
    };
    if (shaped.nodes.length === 0 || shaped.links.length === 0) {
      throw new Error("empty dataset for this date range");
    }
    drawSankey(svg, shaped, 0, shaped.total_kwh_mkl);
    setInfoCards(shaped.total_kwh_mkl);
  } catch (err) {
    if (err.name === "AbortError") return;
    console.error(err);
    d3.select("svg").selectAll("*").remove();
    d3.select("svg").attr("width", width).attr("height", height);
    d3.select("svg").append("text")
      .attr("x", width / 2).attr("y", height / 2)
      .attr("text-anchor", "middle")
      .style("fill", "#e6e9f2")
      .text("Data load failed: " + err.message);
  } finally {
    d3.select("svg").attr("fill-opacity", 1);
  }
}

function updateDiagrams() {
  width = window.innerWidth * 0.9;
  d3.select("svg").selectAll("*").remove();
  d3.select("svg").attr("width", width).attr("height", height);
  drawCurrentGraph();
}

// Control panel
document.querySelectorAll(".button-row").forEach(row => {
  row.addEventListener("click", e => {
    const btn = e.target.closest("button.option");
    if (!btn || btn.disabled) return;
    row.querySelectorAll(".option").forEach(b => b.removeAttribute("data-active"));
    btn.setAttribute("data-active", "");
    selected_sankey_type = btn.textContent === "Demo" ? 0 : 1;
    updateDiagrams();
  });
});

// Control-panel: time-input
document.querySelectorAll(".time-input").forEach((input) => {
  // Hard-block: field can only ever hold HH:MM with minutes in 15-min steps.
  input.addEventListener("input", () => {
    let digits = input.value.replace(/\D/g, "").slice(0, 4);

    // hour can never exceed 23. If the second digit overflows a
    // one-digit hour ("9" + "4"), pad it: 94 -> 094 (= 09:4x)
    if (digits.length === 2 && parseInt(digits, 10) > 23) {
      digits = ("0" + digits).slice(0, 4);
    }

    // minutes must be 00/15/30/45: otherwise drop the last keystroke
    if (digits.length === 4 && parseInt(digits.slice(2), 10) % 15 !== 0) {
      digits = digits.slice(0, 3);
      digits[3] = "0";
    }

    input.value =
      digits.length > 2
        ? digits.slice(0, 2) + ":" + digits.slice(2)
        : digits;
  });

  // End Control-panel //

  // pad on blur: "9" -> "09:00", "14:" -> "14:00"
  input.addEventListener("blur", () => {
    const digits = input.value.replace(/\D/g, "");
    let h = parseInt(digits.slice(0, 2), 10);
    let min = parseInt(digits.slice(2, 4) || "0", 10);
    if (isNaN(h) || h > 23) h = 0;
    if (isNaN(min) || min % 15 !== 0) min = 0;
    input.value =
      String(h).padStart(2, "0") + ":" + String(min).padStart(2, "0");
  });

  // debounce refetch when the time changes
  let debounce = null;
  input.addEventListener("change", () => {
    clearTimeout(debounce);
    debounce = setTimeout(updateDiagrams, 250);
  });
});

document.getElementById("exportBtn").addEventListener("click", () => {
  const api = selected_sankey_type === 1 ? API_FULL : API_REAL;
  window.location = api.replace(/\/api\/.+$/, "") + "/api/export/measurements" + buildQuery();
});

window.onresize = updateDiagrams;
updateDiagrams();
