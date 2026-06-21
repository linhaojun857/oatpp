-- Scenario 9: Deeply nested JSON (3 levels)
wrk.method  = "POST"
wrk.path    = "/nested"
wrk.headers["Content-Type"] = "application/json"
wrk.body    = '{"title":"Root Level","nested":{"name":"Mid Level","child":{"value":"Deep Value","count":7}}}'
