import express from "express";
import mysql from "mysql2/promise";

const app = express();
const port = process.env.PORT || 3000;

const pool = mysql.createPool(
  process.env.DATABASE_URL
    ? { uri: process.env.DATABASE_URL, waitForConnections: true, connectionLimit: 10 }
    : {
      host: process.env.DB_HOST || "mysql",
      user: process.env.DB_USER,
      password: process.env.DB_PASSWORD,
      database: process.env.DB_NAME,
      waitForConnections: true,
      connectionLimit: 10,
    }
);

app.get("/api", (req, res) => {
  res.json({
    endpoints: [
      "GET /api/data-demo",
      "GET /api/data-real",
    ],
  });
});

const SANKEY_NODES = [
  "Mikkelin Kampus",
  "Liittymä 1",
  "Liittymä 2",
  "PK1",
  "RAK01",
  "RAK02",
  "RAK03",
  "RAK04",
  "RAK04",
  "RAK04",
  "RAK04",
  "RAK05",
  "RAK06",
  "RAK07",
  "RAK08",
  "PK10",
  "RAK09",
  "RAK10",
  "RAK11",
  "Muut",
  "RAK09",
  "RAK09",
  "RAK09",
  "RAK09",
  "RAK09",
  "RAK09",
  "RAK12",
  "RAK04",
  "RAK13",
  "Muut",
  "RAK14",
  "RAK15",
  "RAK15",
];

// Parent -> children (indices into SANKEY_NODES). This IS the diagram.
const TREE = {
  0:  [1],
  1:  [3, 2],                                   // PK1 80 %, Liittymä 2 the rest
  2:  [4],                                      // Liittymä 2 -> Mikpoli (was wrongly PK1 -> RAK01)
  4:  [17, 18, 19],                             // Mikpoli -> M, T, Muut2
  3:  [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],  // PK1 -> 10 buildings + PK10
  15: [16, 26, 27, 28, 29],                     // PK10 -> E-rakennus, C NK1, D DNK1, Autolämm PR, Muut
  16: [20, 21, 22, 23, 24, 25],                 // E-rakennus -> its six sub-meters
  29: [30, 31, 32],                             // Muut -> K-rakennus + two H-rakennus meters
};

// Real meter IDs per node. Leave a node out to fall back to demo data for it.
const NODE_METERS = {
  // 5: ["1000101"], 6: ["1000102"], ...
};

// Demo fallback: kWh straight from your reference diagram, scaled by X / 45000.
// Node 24 (E-rakennus 303) set to 2,580 so the six sub-meters sum to E's 11,202.
// H-rakennus totals 429 over two meters — the 215/214 split is a placeholder.
const DEMO = {
  4: 6500, 17: 3050, 18: 2200, 19: 1250,
  5: 2700, 6: 1700, 7: 2700, 8: 2200, 9: 5700, 10: 2700,
  11: 700, 12: 2000, 13: 500, 14: 200,
  16: 11202, 20: 1239, 21: 3857, 22: 255, 23: 1249, 24: 2580, 25: 2022,
  26: 738, 27: 258, 28: 1500, 29: 2924, 30: 1258, 31: 215, 32: 214,
};

app.get("/api/data-full", async (req, res) => {
  const { startDate, endDate } = req.query;
  if (!startDate || !endDate || isNaN(Date.parse(startDate)) || isNaN(Date.parse(endDate))) {
    return res.status(400).json({ error: "required query params: startDate, endDate (YYYY-MM-DD)" });
  }

  const start = `${startDate} 00:00:00`;
  const endDateObj = new Date(endDate);
  endDateObj.setDate(endDateObj.getDate() + 1);
  const end = endDateObj.toISOString().slice(0, 10) + " 00:00:00";

  try {
    // ONE query for every meter in the period
    const [rows] = await pool.query(
      `SELECT meter_id, ROUND(MAX(kwh_import) - MIN(kwh_import), 3) AS total_kwh
       FROM measurements
       WHERE measurement_type = 'BN01'
         AND measurement_time >= ? AND measurement_time < ?
       GROUP BY meter_id`,
      [start, end]
    );
    const kwh = new Map(rows.map((r) => [String(r.meter_id), Number(r.total_kwh) || 0]));

    // Feeder reading drives the demo scale; 45000 keeps the reference picture when empty
    const X = kwh.get("1000000") || 45000;
    const round3 = (n) => Math.round(n * 1000) / 1000;

    // Bottom-up: leaves take real meter sums when mapped, else demo values;
    // every parent is the sum of its children, so the Sankey balances by construction.
    const value = {};
    const calc = (n) => {
      if (value[n] !== undefined) return value[n];
      const kids = TREE[n];
      if (kids) {
        value[n] = round3(kids.reduce((a, c) => a + calc(c), 0));
      } else {
        const meters = NODE_METERS[n];
        const real = meters?.length
          ? meters.reduce((a, id) => a + (kwh.get(id) || 0), 0)
          : 0;
        value[n] = round3(real > 0 ? real : (DEMO[n] || 0) * (X / 45000));
      }
      return value[n];
    };
    calc(0);

    const links = [];
    for (const [src, kids] of Object.entries(TREE)) {
      for (const t of kids) links.push({ source: Number(src), target: t, value: value[t] });
    }

    res.json({ name: "Demo", total_kwh: value[0], nodes: SANKEY_NODES, links });
  } catch (err) {
    console.error("Database error:", err.message);
    res.status(500).json({ error: "Database query failed" });
  }
});

const DEMO_REAL_NODES = [
  "Mikkelin Kampus", // 0
  "PK10",            // 1
  "Muut",            // 2
];

const DEMO_REAL_TREE = {
  0: [1, 2],
};

// Real meter IDs. PK10: the PK10 feeder meter. Muut: everything else, or leave
// empty and it becomes whatever is left over from the campus total.
const DEMO_REAL_METERS = {
  1: [], // PK10
  2: [], // Muut
};

const PK10_SHARE = 0.35; // demo fallback share of campus total

app.get("/api/data-real", async (req, res) => {
  const { startDate, endDate } = req.query;
  if (!startDate || !endDate || isNaN(Date.parse(startDate)) || isNaN(Date.parse(endDate))) {
    return res.status(400).json({ error: "required query params: startDate, endDate (YYYY-MM-DD)" });
  }

  const start = `${startDate} 00:00:00`;
  const endDateObj = new Date(endDate);
  endDateObj.setDate(endDateObj.getDate() + 1);
  const end = endDateObj.toISOString().slice(0, 10) + " 00:00:00";

  try {
    const [rows] = await pool.query(
      `SELECT meter_id, ROUND(MAX(kwh_import) - MIN(kwh_import), 3) AS total_kwh
       FROM measurements
       WHERE measurement_type = 'BN01'
         AND measurement_time >= ? AND measurement_time < ?
       GROUP BY meter_id`,
      [start, end]
    );
    const kwh = new Map(rows.map((r) => [String(r.meter_id), Number(r.total_kwh) || 0]));

    const round3 = (n) => Math.round(n * 1000) / 1000;

    // Campus total: the feeder meter (1000000) when present, else the 45,000 reference.
    const X = round3(kwh.get("1000000") || 45000);

    // PK10: its own meter(s) when mapped, else PK10_SHARE of X.
    const pk10Meters = DEMO_REAL_METERS[1] || [];
    const pk10Real = pk10Meters.reduce((a, id) => a + (kwh.get(id) || 0), 0);

    // Muut: its meter(s) when mapped; otherwise the remainder X - PK10,
    // which guarantees the two branches sum exactly to the campus total.
    const muutMeters = DEMO_REAL_METERS[2] || [];
    const muutReal = muutMeters.reduce((a, id) => a + (kwh.get(id) || 0), 0);

    const pk10 = round3(pk10Real > 0 ? pk10Real : X * PK10_SHARE);
    const muut = round3(muutReal > 0 ? muutReal : X - pk10);

    res.json({
      name: "Reaali",
      total_kwh: X,
      nodes: DEMO_REAL_NODES,
      links: [
        { source: 0, target: 1, value: pk10 },
        { source: 0, target: 2, value: muut },
      ],
    });
  } catch (err) {
    console.error("Database error:", err.message);
    res.status(500).json({ error: "Database query failed" });
  }
});

// Minimal CSV escaping: wrap fields containing quotes/commas/newlines,
// double any embedded quotes (RFC 4180).
const csvField = (v) => {
  const s = v === null || v === undefined ? "" : String(v);
  return /[",\n\r]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
};

app.get("/api/export/measurements", async (req, res) => {
  const { startDate, endDate } = req.query;
  if (!startDate || !endDate || isNaN(Date.parse(startDate)) || isNaN(Date.parse(endDate))) {
    return res.status(400).json({ error: "required query params: startDate, endDate (YYYY-MM-DD)" });
  }

  const start = `${startDate} 00:00:00`;
  const endDateObj = new Date(endDate);
  endDateObj.setDate(endDateObj.getDate() + 1);
  const end = endDateObj.toISOString().slice(0, 10) + " 00:00:00";

  try {
    const [rows] = await pool.query(
      `SELECT meter_id, ROUND(MAX(kwh_import) - MIN(kwh_import), 3) AS total_kwh
       FROM measurements
       WHERE measurement_type = 'BN01'
         AND measurement_time >= ? AND measurement_time < ?
       GROUP BY meter_id ORDER BY meter_id`,
      [start, end]
    );

    const csv = ["meter_id;total_kwh",
      ...rows.map(r => `${r.meter_id};${r.total_kwh}`)].join("\n");

    res.setHeader("Content-Type", "text/csv; charset=utf-8");
    res.setHeader("Content-Disposition",
      `attachment; filename="sankey_${startDate}_${endDate}.csv"`);
    res.send(csv);
  } catch (err) {
    console.error("Export error:", err.message);
    res.status(500).json({ error: "Export failed" });
  }
});

app.listen(port, () => console.log(`API listening on :${port}`));

// Graceful shutdown
async function shutdown() {
  await pool.end();
  console.log("Database pool closed");
  process.exit(0);
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
