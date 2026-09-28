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

app.listen(port, () => console.log(`API listening on :${port}`));

// Graceful shutdown (from your old server.js)
async function shutdown() {
  await pool.end();
  console.log("Database pool closed");
  process.exit(0);
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
