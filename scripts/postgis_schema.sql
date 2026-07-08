-- CarMove PostGIS 默认表结构（与 CarMoveTracker.ini [PostGISDatabase] 默认值一致）
CREATE EXTENSION IF NOT EXISTS postgis;

CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id          BIGSERIAL PRIMARY KEY,
    plate_number        TEXT NOT NULL UNIQUE,
    plate_color         TEXT,
    load_capacity_ton   NUMERIC(8,2),
    owner_name          TEXT,
    operator_company    TEXT,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS trajectory_points (
    id                  BIGSERIAL PRIMARY KEY,
    vehicle_id          BIGINT REFERENCES vehicles(vehicle_id),
    plate_number        TEXT NOT NULL,
    ts                  TIMESTAMPTZ NOT NULL,
    geom                geometry(Point, 4326) NOT NULL,
    speed               REAL,
    direction           SMALLINT,
    total_mileage       TEXT,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_trajectory_plate_ts ON trajectory_points (plate_number, ts);

-- 当前应用按车牌和时间查询轨迹，默认不创建空间索引以提高批量导入速度。
-- 如果后续需要地图范围/空间相交查询，再手动执行：
-- CREATE INDEX IF NOT EXISTS idx_trajectory_geom ON trajectory_points USING GIST (geom);
