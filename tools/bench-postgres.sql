-- compatibility shim
create function last_insert_rowid()
  returns integer as $$
begin
    return lastval();
end; $$ language plpgsql;


create or replace function accum_log_tables()
    returns trigger as
$$
begin
    insert into search_stats
    select slog.execution, stamp_start, stamp, max_states, queued
    from
    (select min(stamp) as stamp_start, execution, max(states) as max_states
     from search_log group by(execution)) smax
    join search_log slog
    on
      slog.execution = smax.execution
      and smax.execution not in (select execution from search_stats)
      and states = max_states
    join job
    on job.execution = smax.execution
    where job.status = 'D';

    insert into pool_stats
    select plog.execution, stamp_start, stamp, pool, items, max_used, held
    from
    (select min(stamp) as stamp_start, execution, pool, max(used) as max_used
     from pool_log group by(execution,pool)) pmax
    join pool_log plog
    on
      plog.execution = pmax.execution
      and (pmax.execution, pmax.pool) not in (select execution, pool from pool_stats)
      and used = max_used
    join job
    on job.execution = pmax.execution
    where job.status = 'D';

    delete from search_log where stamp < NOW() - interval '2 weeks';
    delete from  pool_log  where stamp < NOW() - interval '2 weeks';

    return new;
end;
$$ language plpgsql;


create trigger job_done
after update on job
execute function accum_log_tables();
