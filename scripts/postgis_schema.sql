-- CarMove PostGIS 默认表结构（日历天航迹 + 轨迹点，轨迹表不冗余车牌）
CREATE EXTENSION IF NOT EXISTS postgis;

CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id          BIGSERIAL PRIMARY KEY,
    plate_number        TEXT NOT NULL UNIQUE,
    plate_color         TEXT,
    load_capacity_ton   NUMERIC(8,2),
    owner_name          TEXT,
    operator_company    TEXT,
    day_count           INT NOT NULL DEFAULT 0,
    total_point_count   BIGINT NOT NULL DEFAULT 0,
    first_seen_at       TIMESTAMPTZ,
    last_seen_at        TIMESTAMPTZ,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS trajectory_days (
    trajectory_day_id   BIGSERIAL PRIMARY KEY,
    vehicle_id          BIGINT NOT NULL REFERENCES vehicles(vehicle_id) ON DELETE CASCADE,
    trajectory_date     DATE NOT NULL,
    first_ts            TIME NOT NULL,
    last_ts             TIME NOT NULL,
    point_count         INT NOT NULL DEFAULT 0,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT uq_trajectory_days_vehicle_date UNIQUE (vehicle_id, trajectory_date),
    CONSTRAINT chk_trajectory_days_ts_order CHECK (last_ts >= first_ts)
);

CREATE TABLE IF NOT EXISTS trajectory_points (
    id                  BIGSERIAL PRIMARY KEY,
    trajectory_day_id   BIGINT NOT NULL
                            REFERENCES trajectory_days(trajectory_day_id) ON DELETE CASCADE,
    ts                  TIME NOT NULL,
    geom                geometry(Point, 4326) NOT NULL,
    speed               REAL,
    direction           SMALLINT,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_trajectory_days_vehicle_date
    ON trajectory_days (vehicle_id, trajectory_date);

CREATE INDEX IF NOT EXISTS idx_trajectory_points_day_ts
    ON trajectory_points (trajectory_day_id, ts);
