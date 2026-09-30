function setInfoCards(yearly_kwh_total) {
  const val0 = Math.round(yearly_kwh_total);
  const val1 = Math.round(yearly_kwh_total / 12);
  const val2 = Math.round(yearly_kwh_total / 365.25);
  const val3 = Math.round(yearly_kwh_total / 365.25 / 24);
  document.getElementsByClassName("info-cards")[0].children[0].getElementsByClassName("val")[0].textContent = `${val0}`;
  document.getElementsByClassName("info-cards")[0].children[1].getElementsByClassName("val")[0].textContent = `${val1}`;
  document.getElementsByClassName("info-cards")[0].children[2].getElementsByClassName("val")[0].textContent = `${val2}`;
  document.getElementsByClassName("info-cards")[0].children[3].getElementsByClassName("val")[0].textContent = `${val3}`;
}

(function () {
  var YEAR_MIN = 2024, YEAR_MAX = 2026;

  var wrap = document.getElementById('sliderWrap');
  var track = document.getElementById('sliderTrack');
  var fill = document.getElementById('sliderFill');
  var hStart = document.getElementById('handleStart');
  var hEnd = document.getElementById('handleEnd');
  var ticks = document.getElementById('tickLabels');

  var dateStart = document.getElementById('dateStart');
  var dateEnd = document.getElementById('dateEnd');
  var yearSelector = document.getElementById('yearSelector');

  // Two months shown across the slider for the selected year
  var state = { year: 2024, start: 0, end: 0.4 }; // start/end as 0..1 of the year

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

  function applyDrag(frac) {
    var minGap = 0.01;
    if (activeHandle === hStart) {
      state.start = Math.min(frac, state.end - minGap);
    } else {
      state.end = Math.max(frac, state.start + minGap);
    }
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
  }

  wrap.addEventListener('pointerdown', onPointerDown);

  // Keyboard support for handles
  function keyStep(handle, delta) {
    if (handle === hStart) {
      state.start = Math.min(1, Math.max(0, state.start + delta));
      if (state.start > state.end - 0.01) state.start = state.end - 0.01;
    } else {
      state.end = Math.min(1, Math.max(0, state.end + delta));
      if (state.end < state.start + 0.01) state.end = state.start + 0.01;
    }
    render();
  }

  [hStart, hEnd].forEach(function (handle) {
    handle.addEventListener('keydown', function (e) {
      var step = e.shiftKey ? 1 / 12 : 1 / 96; // shift = one month
      if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') { e.preventDefault(); keyStep(handle, -step); }
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') { e.preventDefault(); keyStep(handle, step); }
    });
  });

  // --- Year selector: switch view to the chosen year ---
  yearSelector.addEventListener('click', function (e) {
    var btn = e.target.closest('.year-btn');
    if (!btn) return;
    yearSelector.querySelectorAll('.year-btn').forEach(function (b) {
      b.classList.remove('active');
    });
    btn.classList.add('active');
    state.year = parseInt(btn.dataset.year, 10);

    // Reset the range to the full year and sync date inputs
    state.start = 0;
    state.end = 1;
    render();
    dateStart.value = state.year + '-01-01';
    dateEnd.value = state.year + '-12-31';
    render();

    scrollToActiveYear();
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
    if (!isNaN(s) && s.getFullYear() === state.year) state.start = dateToFrac(s);
    if (!isNaN(f) && f.getFullYear() === state.year) state.end = dateToFrac(f);
    if (state.end - state.start < 0.01) state.end = Math.min(1, state.start + 0.01);
    render();
  }

  dateStart.addEventListener('change', syncFromDates);
  dateEnd.addEventListener('change', syncFromDates);

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

const savorex_json =
  [
    // graph_mkl_demo //
    // 2024
    // ...
    // 2025
    // ...
    // 2026:
    {
      name: "Demo",
      total_kwh_mkl: 1474627 * 1.851851852 * 1.78,
      nodes: [
        { name: "Mikkelin Kampus" },                                    // Muuntamo
        { name: "PK" },                                                 // PK
        { name: "PK1" },                                                // PK1
        { name: "Mikpoli" },                                            // Mikpoli
        { name: "A-rakennus LNK1P" },
        { name: "B-rakennus" },
        { name: "D-rakennus JKD1" },
        { name: "D-rakennus LNK1N" },
        { name: "D-rakennus jäähdytys, IJK" },
        { name: "D:n keittiö" },
        { name: "F-rakennus" },
        { name: "X-rakennus" },
        { name: "Autolämmitykset" },
        { name: "KJK1" },
        { name: "PK10" },                                               // PK10
        { name: "E-rakennus" },
        { name: "E-rakennus ERK002" },
        { name: "E-rakennus ERK201" },
        { name: "E-rakennus ERK302" },
        { name: "E-rakennus ERK401" },
        { name: "E-rakennus ERK205" },
        { name: "E-rakennus ERK303" },
        { name: "E-rakennus ERK001 ja 101 sekä JKE0.1-0.4" },
        { name: "C-rakennus NK1" },
        { name: "D-rakennus DNK1" },
        { name: "Autolämmitys PR-keskus" },
        { name: "H+K-rakennukset" },                                    // H-K-rakennukset
        { name: "K-rakennus" },
        { name: "H-rakennus" },
        { name: "Muut" },                                               // PK10 muut
        { name: "Mikpoli" },                                               // PK Mikpoli
        { name: "M-rakennus" },
        { name: "T-rakennus" },
        { name: "Muut" }                                                // Mikpoli muut
      ],
      links: [
        { source: 0, target: 1, value: 1474627 * 1.851851852 * 1.78 },                       // OSTO => PK
        { source: 1, target: 2, value: 1474627 * 1.851851852 * 1.78 * 0.8 },                       // PK => PK1 80%
        { source: 2, target: 4, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => A-rakennus LNK1P
        { source: 2, target: 5, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => B-rakennus
        { source: 2, target: 6, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => D-rakennus JKD1
        { source: 2, target: 7, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => D-rakennus LNK1N
        { source: 2, target: 8, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => D-rakennus jäähdytys, IJK
        { source: 2, target: 9, value: 1474627 * 1.851851852 * 0.1 },                   // PK1 => D:n keittiö
        { source: 2, target: 10, value: 1474627 * 1.851851852 * 0.1 },                  // PK1 => F-rakennus
        { source: 2, target: 11, value: 1474627 * 1.851851852 * 0.1 },                  // PK1 => X-rakennus
        { source: 2, target: 12, value: 1474627 * 1.851851852 * 0.1 },                  // PK1 => Autolämmitys
        { source: 2, target: 13, value: 1474627 * 1.851851852 * 0.1 },                  // PK1 => KJK1
        { source: 2, target: 14, value: 1474627 },                                      // PK1 => PK10
        { source: 14, target: 15, value: 1474627 * 0.4 },                               // PK10 => E-rakennus
        { source: 15, target: 16, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK002
        { source: 15, target: 17, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK201
        { source: 15, target: 18, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK302
        { source: 15, target: 19, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK401
        { source: 15, target: 20, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK205
        { source: 15, target: 21, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK303
        { source: 15, target: 22, value: 1474627 * 0.4 * 0.1429 },                      // E-rakennus => E-rakennus ERK001 ja 101 sekä JKE0.1-0.4
        { source: 14, target: 23, value: 1474627 * 0.36 * 0.35 },                       // PK10 => C-rakennus NK1
        { source: 14, target: 24, value: 1474627 * 0.36 * 0.15 },                       // PK10 => D-rakennus DNK1
        { source: 14, target: 25, value: 1474627 * 0.36 * 0.1 },                        // PK10 => Autolämmitys PR-keskus
        { source: 14, target: 26, value: 1474627 * 0.36 * 0.5 },                        // PK10 => H-K-rakennukset
        { source: 26, target: 27, value: 1474627 * 0.36 * 0.5 * 0.7 },                  // H-K-rakennukset => K-rakennus
        { source: 26, target: 28, value: 1474627 * 0.36 * 0.5 * 0.3 },                  // H-K-rakennukset => H-rakennus
        { source: 14, target: 29, value: 1474627 * 0.02 },                              // PK10 muut
        { source: 1, target: 3, value: 1474627 * 1.851851852 * 1.78 * 0.2 },                       // PK => Mikpoli 20%
        { source: 3, target: 4, value: 1474627 * 1.851851852 * 1.78 * 0.2 },
        { source: 4, target: 30, value: 1474627 * 1.851851852 * 1.78 * 0.2 * 0.6 },    // Mikpoli => M-rakennus 60%
        { source: 4, target: 31, value: 1474627 * 1.851851852 * 1.78 * 0.2 * 0.3 },    // Mikpoli => T-rakennus 30%
        { source: 4, target: 32, value: 1474627 * 1.851851852 * 1.78 * 0.2 * 0.1 }     // Mikpoli => muut 10%
      ]
    },
    // graph_mkl_reaali //
    {
      name: "Reaali",
      total_kwh_mkl: 16220897,
      nodes: [
        { name: "Mikkelin Kampus" },
        { name: "PK" },
        { name: "E-Rakennus (PK10)" },
        { name: "Muu kampus" }
      ],
      links: [
        { source: 0, target: 1, value: 16220897 },
        { source: 1, target: 2, value: 1474627 },
        { source: 1, target: 3, value: 16220897 - 1474627 }
      ]
    }
  ];

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
    .text("XAMK Energy");

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

function drawDemoGraph() {
  const data = savorex_json[0];
  drawSankey(svg, data, 0, data.total_kwh_mkl);
  setInfoCards(data.total_kwh_mkl);
}

function drawReaaliGraph() {
  const data = savorex_json[1];
  drawSankey(svg, data, 0, data.total_kwh_mkl);
  setInfoCards(data.total_kwh_mkl);
}

function updateDiagrams() {
  width = window.innerWidth * 0.9;
  d3.select("svg").selectAll("*").remove();
  d3.select("svg").attr("width", width).attr("height", height);
  if (selected_sankey_type == 0) {
    drawDemoGraph();
  } else {
    drawReaaliGraph();
  }
}

// Control panel
document.querySelectorAll(".button-row").forEach(row => {
  row.addEventListener("click", e => {
    const btn = e.target.closest("button.option");
    if (!btn || btn.disabled) return;
    row.querySelectorAll(".option").forEach(b => b.removeAttribute("data-active"));
    btn.setAttribute("data-active", "");
    if (btn.textContent == "Demo") {
      selected_sankey_type = 0;
    } else {
      selected_sankey_type = 1;
    }
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
});

window.onresize = updateDiagrams;
updateDiagrams();