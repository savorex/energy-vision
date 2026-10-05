SET SESSION cte_max_recursion_depth = 100000;

INSERT INTO measurements
    (measurement_time, meter_id, kwh_import, kwh_export,
     kwh_production, kwh_consumption, source_type, status_type, measurement_type)
WITH RECURSIVE quarters AS (
    SELECT TIMESTAMP('2024-01-01 00:00:00') AS ts
    UNION ALL
    SELECT ts + INTERVAL 15 MINUTE FROM quarters
    WHERE ts < NOW() - INTERVAL MOD(MINUTE(NOW()), 15) MINUTE
),
meters_seed AS (
    SELECT
        m.meter_id,
        CAST(SUBSTRING(m.identifier, 5) AS UNSIGNED) AS simnum,
        (CAST(SUBSTRING(m.identifier, 5) AS UNSIGNED) * 7919) % 997 AS seed0,
        CASE
          WHEN CAST(SUBSTRING(m.identifier, 5) AS UNSIGNED) <= 22
            THEN TIMESTAMP('2024-01-01 00:00:00')
          ELSE TIMESTAMP('2025-07-01 00:00:00')
               + INTERVAL ((CAST(SUBSTRING(m.identifier, 5) AS UNSIGNED) * 37) % 4400) HOUR
        END AS start_ts
    FROM meters m
    WHERE m.identifier LIKE 'sim-%'
),
profile AS (
    SELECT
        q.ts,
        ms.meter_id,
        ms.seed0,
        ms.start_ts,
        CASE t.btype WHEN 'BN01' THEN 1 WHEN 'BN02' THEN 263 ELSE 523 END AS toff,
        FLOOR(HOUR(q.ts) * 4 + MINUTE(q.ts) / 15) AS qod,
        MONTH(q.ts) AS mo
    FROM quarters q
    CROSS JOIN meters_seed ms
    CROSS JOIN (SELECT 'BN01' btype UNION ALL SELECT 'BN02' UNION ALL SELECT 'BN03') t
    WHERE q.ts >= ms.start_ts
),
power AS (
    SELECT
        ts, meter_id, seed0, toff, qod, mo,
        CASE WHEN toff = 1 THEN 1.0 WHEN toff = 263 THEN 0.45 ELSE 1.35 END AS scale,
        (
          0.2 + (seed0 % 40) / 100.0
          + CASE WHEN qod BETWEEN 24 AND 35 THEN 1.5 + (seed0 % 20) / 20.0 ELSE 0 END
          + CASE WHEN qod BETWEEN 68 AND 83 THEN 2.2 + (seed0 % 30) / 15.0 ELSE 0 END
          + ((seed0 * (qod * 31 + DAYOFYEAR(ts) * 7)) % 50) / 400.0
        ) * CASE WHEN mo IN (11,12,1,2,3) THEN 1.6 ELSE 1.0 END
        AS delta_raw
    FROM profile
),
agg AS (
    SELECT
        ts, meter_id, toff,
        SUM(delta_raw * scale) OVER (PARTITION BY meter_id, toff ORDER BY ts) AS kwh_cum
    FROM power
)
SELECT
    a.ts,
    a.meter_id,
    ROUND(a.kwh_cum, 3),
    0, 0, 0,
    'sim',
    'ok',
    CASE a.toff WHEN 1 THEN 'BN01' WHEN 263 THEN 'BN02' ELSE 'BN03' END
FROM agg a;