-- Scenario 7: Multiple custom headers
wrk.method  = "GET"
wrk.path    = "/headers"
wrk.headers["X-Header-1"] = "header-value-one"
wrk.headers["X-Header-2"] = "42"
wrk.headers["X-Header-3"] = "3.14"
