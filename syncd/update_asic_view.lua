local function removeKeys(keys)
    local n = table.getn(keys)

    for i = 1, n do
        redis.call('DEL', keys[i])
    end
end

removeKeys(redis.call('KEYS', KEYS[1])) -- asic view
removeKeys(redis.call('KEYS', KEYS[2])) -- asic temp view

local function split(str, sep)
    local fields = {}
    local pattern = string.format("([^%s]+)", sep)
    str:gsub(pattern, function(c) fields[#fields+1] = c end)
    return fields
end

local kfv = KEYS[3] -- key|field|value|key|field|value|...

local arr = split(kfv,"|")

local n = table.getn(arr)

for i = 1, n, 3 do
    redis.call('HSET', arr[i], arr[i+1], arr[i+2])
end
