-- Scenario 3: Large JSON request/response (15-field DTO)
wrk.method  = "POST"
wrk.path    = "/json/large"
wrk.headers["Content-Type"] = "application/json"
wrk.body    = [[
{
  "field1": "Lorem ipsum dolor sit amet consectetur adipiscing elit",
  "field2": 1234567890,
  "field3": 3.141592653589793,
  "field4": true,
  "field5": "Sed ut perspiciatis unde omnis iste natus error sit voluptatem",
  "field6": "At vero eos et accusamus et iusto odio dignissimos ducimus",
  "field7": "Excepteur sint occaecat cupidatat non proident sunt in culpa",
  "field8": 987654321,
  "field9": 555777,
  "field10": 2.718281828459045,
  "field11": [1,2,3,4,5,6,7,8,9,10],
  "field12": ["alpha","beta","gamma","delta","epsilon"],
  "field13": 1.618,
  "field14": 9223372036854775807,
  "field15": false
}
]]
