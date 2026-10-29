SET GLOBAL event_scheduler = ON;
SET SESSION cte_max_recursion_depth = 50000;

DELIMITER //

DROP PROCEDURE IF EXISTS sim_tick //

CREATE PROCEDURE sim_tick()
BEGIN
  DECLARE now_floor DATETIME;
  SET now_floor = TIMESTAMP(DATE_FORMAT(NOW(), '%Y-%m-%d %H:'));  -- top of current hour

  -- Cap: 90 days of history per meter per call. Small committed transactions;
  -- repeat CALL sim_tick(); until caught up (or let the hourly event do it).
  INSERT INTO measurements
      (measurement_time, meter_id, kwh_import, kwh_export,
       kwh_production, kwh_consumption, source_type, status_type, measurement_type)
  WITH RECURSIVE
  starts AS (
    SELECT
      m.meter_id,
      (m.meter_id * 7919) % 997 AS seed0,
      GREATEST(
        COALESCE(
          (SELECT MAX(x.measurement_time) FROM measurements x
           WHERE x.meter_id = m.meter_id AND x.measurement_type = 'BN01'),
          CASE
            WHEN m.meter_id <= 3
              THEN TIMESTAMP('2024-01-01 00:00:00')
            ELSE TIMESTAMP('2026-01-01 00:00:00')
                 + INTERVAL ((m.meter_id - 3) * 37) % 220 DAY
          END
        ),
        CASE
          WHEN m.meter_id <= 3
            THEN TIMESTAMP('2024-01-01 00:00:00')
          ELSE TIMESTAMP('2026-01-01 00:00:00')
               + INTERVAL ((m.meter_id - 3) * 37) % 220 DAY
        END
      ) AS from_ts
    FROM meters m
  ),
  slots AS (
    SELECT meter_id, seed0, from_ts, from_ts + INTERVAL 1 HOUR AS ts
    FROM starts
    WHERE from_ts + INTERVAL 1 HOUR <= LEAST(now_floor, from_ts + INTERVAL 2160 HOUR)  -- 90-day chunk
    UNION ALL
    SELECT meter_id, seed0, from_ts, ts + INTERVAL 1 HOUR
    FROM slots
    WHERE ts + INTERVAL 1 HOUR <= LEAST(now_floor, from_ts + INTERVAL 2160 HOUR)
  ),
  power AS (
    SELECT
      ts, meter_id, seed0, from_ts,
      HOUR(ts) AS hod,
      MONTH(ts) AS mo,
      DAYOFWEEK(ts) AS dow
    FROM slots
  ),
  value AS (
    SELECT
      ts, meter_id, from_ts,
      -- base load + morning/evening peaks + weekend dip + winter factor + noise
      (
        0.4 + (seed0 % 7) * 0.06
        + CASE WHEN hod BETWEEN 6 AND 9   THEN 0.8 + (seed0 % 5) * 0.08 ELSE 0 END
        + CASE WHEN hod BETWEEN 16 AND 21 THEN 1.4 + (seed0 % 9) * 0.09 ELSE 0 END
        + CASE WHEN dow IN (1, 7) THEN -0.3 ELSE 0 END
        + CASE WHEN mo IN (11, 12, 1, 2, 3) THEN 0.5 ELSE 0 END
        + ((seed0 * (hod * 31 + DAYOFYEAR(ts) * 7)) % 20) / 25.0
      ) AS kwh
    FROM power
  ),
  agg AS (
    SELECT
      v.ts, v.meter_id, v.from_ts,
      COALESCE(
        (SELECT MAX(x.kwh_import) FROM measurements x
         WHERE x.meter_id = v.meter_id AND x.measurement_type = 'BN01'
           AND x.measurement_time <= v.from_ts), 0)
      + SUM(v.kwh) OVER (PARTITION BY v.meter_id ORDER BY v.ts) AS kwh_cum
    FROM value v
  )
  SELECT
    a.ts,
    a.meter_id,
    ROUND(a.kwh_cum, 3),   -- cumulative register, so MAX-MIN aggregation works
    0, 0, 0,
    'sim',
    'ok',
    'BN01'
  FROM agg a;
END //

DROP EVENT IF EXISTS sim_hourly_tick //

CREATE EVENT sim_hourly_tick
ON SCHEDULE EVERY 1 HOUR
STARTS (TIMESTAMP(DATE_FORMAT(NOW(), '%Y-%m-%d %H:')) + INTERVAL 5 MINUTE)
DO CALL sim_tick(); //

DELIMITER ;

-- First chunk now. The 2024-meters need ~10 calls, the 2026-meters ~3-4;
-- CALL sim_tick(); repeatedly until COUNT(*) plateaus.
CALL sim_tick();
