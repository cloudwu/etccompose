local etc = require "etc"

local f = io.open("icon.ktx", "rb")
local data = f:read "a"
f:close()

local img = etc.info(data)

local d = etc.compose {
	{ data, 0, 0 },
	{ data, img.width, 0 },
	{ data, 0, img.height },
	{ data, img.width, img.height },
}

img = etc.info(d)

local f = io.open("icon4.ktx", "wb")
f:write(d)
f:close()


