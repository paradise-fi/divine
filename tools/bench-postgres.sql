-- compatibility shim
create function last_insert_rowid()
  returns integer as $$
begin
    return lastval();
end; $$ language plpgsql;
