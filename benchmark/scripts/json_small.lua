-- Scenario 2: Small JSON request/response
wrk.method  = "POST"
wrk.path    = "/json"
wrk.headers["Content-Type"] = "application/json"
wrk.body    = '{"message":"hello benchmark","value":42,"number":3.14159}'
