-- Scenario 6: Plain text body echo
wrk.method  = "POST"
wrk.path    = "/echo"
wrk.headers["Content-Type"] = "text/plain"
wrk.body    = "The quick brown fox jumps over the lazy dog 1234567890"
