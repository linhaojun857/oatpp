-- Scenario 8: Mixed-type DTO (String, Int, Bool, Vector, Enum)
wrk.method  = "POST"
wrk.path    = "/mixed"
wrk.headers["Content-Type"] = "application/json"
wrk.body    = '{"name":"John Doe","age":30,"score":95.5,"active":true,"tags":["user","premium","vip"],"favoriteColor":"BLUE"}'
