local r2v = KEYS[1]
local v2r = KEYS[2]

redis.call('DEL', r2v)
redis.call('DEL', v2r)

local oids = KEYS[3] -- rid,vid,rid,vid,...

local function split(str, sep)
    local fields = {}
    local pattern = string.format("([^%s]+)", sep)
    str:gsub(pattern, function(c) fields[#fields+1] = c end)
    return fields
end

local arr = split(oids,",")

local n = table.getn(arr)

for i = 1, n, 2 do
    redis.call('HSET', r2v, arr[i], arr[i+1])
    redis.call('HSET', v2r, arr[i+1], arr[i])
end
