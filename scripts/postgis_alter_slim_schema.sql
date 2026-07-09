-- 精简 trajectory_days / trajectory_points 字段（已有库升级，执行前请备份）
--
-- 空间说明：
--   DROP COLUMN / 改类型后，PostgreSQL 不会立刻把磁盘文件缩小。
--   VACUUM ANALYZE 回收表内 dead tuple，供后续 INSERT/UPDATE 复用，并刷新查询统计。
--   若要把空间真正还给操作系统，需在维护窗口单独执行 VACUUM FULL（见文末，大表会锁表且耗时）。

ALTER TABLE trajectory_days DROP COLUMN IF EXISTS source_file;

ALTER TABLE trajectory_points DROP COLUMN IF EXISTS total_mileage;

ALTER TABLE trajectory_days
    ALTER COLUMN first_ts TYPE TIME USING (first_ts AT TIME ZONE 'Asia/Shanghai')::time,
    ALTER COLUMN last_ts TYPE TIME USING (last_ts AT TIME ZONE 'Asia/Shanghai')::time;

ALTER TABLE trajectory_points
    ALTER COLUMN ts TYPE TIME USING (ts AT TIME ZONE 'Asia/Shanghai')::time;

VACUUM ANALYZE trajectory_days;
VACUUM ANALYZE trajectory_points;
VACUUM ANALYZE vehicles;

-- 可选：维护窗口、可接受锁表时再执行（2000 万点级 trajectory_points 可能需数小时）
-- VACUUM FULL VERBOSE trajectory_points;
-- VACUUM FULL VERBOSE trajectory_days;
