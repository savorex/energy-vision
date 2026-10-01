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
      "GET /api/charts",
      "GET /api/charts/:id/flows",
      "GET /api/charts/:id/info",
      "GET /api/charts/:id/panel",
    ],
  });
});

// List available charts (for the buttons)
app.get("/api/charts", async (req, res) => {
  const [rows] = await pool.query(
    "SELECT DISTINCT chart_id FROM sankey_flows ORDER BY chart_id"
  );
  res.json(rows.map((r) => r.chart_id));
});

// Flows for one chart
app.get("/api/charts/:id/flows", async (req, res) => {
  try {
    const [rows] = await pool.query(
      "SELECT node_from, node_to, value, color FROM sankey_flows WHERE chart_id = ?",
      [req.params.id]
    );
    if (rows.length === 0) return res.status(404).json({ error: "chart not found" });
    res.json(rows);
  } catch (err) {
    console.error("Database error:", err.message);
    res.status(500).json({ error: "Database query failed" });
  }
});

// Chart metadata
app.get("/api/charts/:id/info", async (req, res) => {
  try {
    const [rows] = await pool.query("SELECT * FROM sankey_charts WHERE id = ?", [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: "chart not found" });
    res.json(rows[0]);
  } catch (err) {
    console.error("Database error:", err.message);
    res.status(500).json({ error: "Database query failed" });
  }
});

// Everything the control panel needs, in one call
app.get("/api/charts/:id/panel", async (req, res) => {
  try {
    const [[chart]] = await pool.query("SELECT * FROM sankey_charts WHERE id = ?", [req.params.id]);
    const [flows] = await pool.query(
      "SELECT node_from, node_to, value, color FROM sankey_flows WHERE chart_id = ?",
      [req.params.id]
    );
    if (!chart && flows.length === 0) return res.status(404).json({ error: "chart not found" });

    // Node totals: sum per node, split by role (source = only sends, sink = only receives)
    const totals = new Map();
    for (const f of flows) {
      totals.set(f.node_from, { name: f.node_from, out: (totals.get(f.node_from)?.out || 0) + f.value });
      totals.set(f.node_to, { name: f.node_to, in: (totals.get(f.node_to)?.in || 0) + f.value });
    }
    const nodes = [...totals.values()].map((n) => ({
      name: n.name,
      in: n.in || 0,
      out: n.out || 0,
      role: n.in && n.out ? "transit" : n.out ? "source" : "sink",
    }));

    const totalIn = nodes.reduce((s, n) => s + n.in, 0);
    const totalOut = nodes.reduce((s, n) => s + n.out, 0);

    res.json({
      chart: chart || { id: req.params.id, title: req.params.id, unit: "" },
      flows,
      nodes,
      totals: {
        in: totalIn,
        out: totalOut,
        imbalance: Math.round((totalIn - totalOut) * 1000) / 1000,
      },
    });
  } catch (err) {
    console.error("Database error:", err.message);
    res.status(500).json({ error: "Database query failed" });
  }
});

app.listen(port, () => console.log(`API listening on :${port}`));

// Graceful shutdown (from your old server.js)
async function shutdown() {
  await pool.end();
  console.log("Database pool closed");
  process.exit(0);
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
