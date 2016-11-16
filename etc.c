#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>

#define ETC2_EAC_BLOCKSIZE 16
#define ETC_BLOCK 4

struct etc_file {
	uint8_t identifier[12];
	uint32_t endianness;
	uint32_t glType;
	uint32_t glTypeSize;
	uint32_t glFormat;
	uint32_t glInternalFormat;
	uint32_t glBaseInternalFormat;
	uint32_t pixelWidth;
	uint32_t pixelHeight;
	uint32_t pixelDepth;
	uint32_t numberOfArrayElements;
	uint32_t numberOfFaces;
	uint32_t numberOfMipmapLevels;
	uint32_t bytesOfKeyValueData;
};

static void
endian(uint32_t *v) {
	union {
		uint32_t v;
		uint8_t b[4];
	} n1,n2;
	n1.v = *v;
	n2.b[0] = n1.b[3];
	n2.b[1] = n1.b[2];
	n2.b[2] = n1.b[1];
	n2.b[3] = n1.b[0];
	*v = n2.v;
}

static const char etc_tag[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static int
letc_info(lua_State *L) {
	size_t sz;
	const char * etc_raw = luaL_checklstring(L, 1, &sz);
	if (sz < sizeof(struct etc_file)) {
		return luaL_error(L, "Invalid etc header size");
	}
	struct etc_file etc_header = *(const struct etc_file *)etc_raw;
	if (memcmp(etc_tag, etc_header.identifier, sizeof(etc_tag)) != 0) {
		return luaL_error(L, "Invalid etc identifier");
	}
	if (etc_header.endianness == 0x1020304) {
		endian(&etc_header.glType);
		endian(&etc_header.glTypeSize);
		endian(&etc_header.glFormat);
		endian(&etc_header.glInternalFormat);
		endian(&etc_header.glBaseInternalFormat);
		endian(&etc_header.pixelWidth);
		endian(&etc_header.pixelHeight);
		endian(&etc_header.pixelDepth);
		endian(&etc_header.numberOfArrayElements);
		endian(&etc_header.numberOfFaces);
		endian(&etc_header.numberOfMipmapLevels);
		endian(&etc_header.bytesOfKeyValueData);
	}

	uint32_t meta_size = (etc_header.bytesOfKeyValueData + 3) & ~3;
	if (sz < sizeof(struct etc_file) + meta_size) {
		return luaL_error(L, "Invalid etc meta data size %u", etc_header.bytesOfKeyValueData);
	}
	uint32_t image_size = *(uint32_t *)(etc_raw + sizeof(struct etc_file) + meta_size);
	if (etc_header.endianness == 0x1020304) {
		endian(&image_size);
	}
	if (sizeof(struct etc_file) + meta_size + image_size + sizeof(image_size) != sz) {
		return luaL_error(L, "Invalid image size (%d)", image_size);
	}

	if (etc_header.glType != 0 || 
		etc_header.glTypeSize !=1 ||
		etc_header.glFormat != 0 ) {
		return luaL_error(L, "Only support compressd data");
	}
	if (etc_header.pixelDepth != 0) {
		return luaL_error(L, "pixelDepth must be 0 (%u)", etc_header.pixelDepth);
	}
	if (etc_header.numberOfArrayElements != 0) {
		return luaL_error(L, "Don't support array texture (%u)", etc_header.numberOfArrayElements);
	}
	if (etc_header.numberOfFaces != 1) {
		return luaL_error(L, "Don't support cube texture (%u)", etc_header.numberOfFaces);
	}
	if (etc_header.numberOfMipmapLevels != 1) {
		return luaL_error(L, "Don't support mipmap (%u)", etc_header.numberOfMipmapLevels);
	}

#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_RGB8_ETC2 0x9274

	if (etc_header.glBaseInternalFormat != GL_RGBA ||
		etc_header.glInternalFormat != GL_COMPRESSED_RGBA8_ETC2_EAC) {
		return luaL_error(L, "Only support RGBA8 texture (%d %d)", 
			etc_header.glInternalFormat,
			etc_header.glBaseInternalFormat);
	}
	lua_newtable(L);
	lua_pushinteger(L, (etc_header.pixelWidth + ETC_BLOCK - 1) / ETC_BLOCK);
	lua_setfield(L, -2, "width");
	lua_pushinteger(L, (etc_header.pixelHeight + ETC_BLOCK -1) / ETC_BLOCK);
	lua_setfield(L, -2, "height");
	lua_pushinteger(L, image_size);
	lua_setfield(L, -2, "image_size");

	if (etc_header.endianness == 0x1020304) {
		endian(&etc_header.glType);
		endian(&etc_header.glTypeSize);
		endian(&etc_header.glFormat);
		endian(&etc_header.glInternalFormat);
		endian(&etc_header.glBaseInternalFormat);
		endian(&etc_header.pixelWidth);
		endian(&etc_header.pixelHeight);
		endian(&etc_header.pixelDepth);
		endian(&etc_header.numberOfArrayElements);
		endian(&etc_header.numberOfFaces);
		endian(&etc_header.numberOfMipmapLevels);
		endian(&etc_header.bytesOfKeyValueData);
	}
	return 1;
}

struct etc_header {
	uint32_t width;
	uint32_t height;
	uint32_t offset;
};

static void
get_header(const char *rawdata, struct etc_header *eh) {
	const struct etc_file *etc_header = (const struct etc_file *)rawdata;
	uint32_t w = etc_header->pixelWidth;
	uint32_t h = etc_header->pixelHeight;
	uint32_t metasize = etc_header->bytesOfKeyValueData;

	if (etc_header->endianness == 0x1020304) {
		endian(&w);
		endian(&h);
		endian(&metasize);
	}
	eh->width = (w + ETC_BLOCK - 1)/ ETC_BLOCK;
	eh->height = (h + ETC_BLOCK - 1)/ ETC_BLOCK;
	eh->offset = sizeof(struct etc_file) + ((metasize + 3) & ~3) + sizeof(uint32_t);
}

static int
letc_offset(lua_State *L) {
	const char * raw = luaL_checkstring(L,1);
	int x = luaL_checkinteger(L, 2);
	int y = luaL_checkinteger(L, 3);
	struct etc_header eh;
	get_header(raw, &eh);
	if (x < 0 || x >= eh.width) {
		return luaL_error(L, "invalid x (x=%d width=%d)", x, (int)eh.width);
	}
	if (y < 0 || y >= eh.height) {
		return luaL_error(L, "invalid y (y=%d height=%d)", y, (int)eh.height);
	}
	lua_pushinteger(L, (eh.width * y + x) * ETC2_EAC_BLOCKSIZE + eh.offset);
	return 1;
}

static void
fill_etc_fileheader(struct etc_file *ef, int w, int h) {
	memcpy(ef->identifier , etc_tag, sizeof(etc_tag));
	ef->endianness = 0x04030201;
	ef->glType = 0;
	ef->glTypeSize = 1;
	ef->glFormat = 0;
	ef->glInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
	ef->glBaseInternalFormat = GL_RGBA;
	ef->pixelWidth = w * ETC_BLOCK;
	ef->pixelHeight = h * ETC_BLOCK;
	ef->pixelDepth = 0;
	ef->numberOfArrayElements = 0;
	ef->numberOfFaces = 1;
	ef->numberOfMipmapLevels = 1;
	ef->bytesOfKeyValueData = 0;
}

static int
letc_compose(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	int i;
	int width=0, height=0;
	for (i=0;;++i) {
		int t = lua_geti(L, 1, i+1);
		if (t == LUA_TNIL) {
			lua_pop(L, 1);
			break;
		}
		if (t != LUA_TTABLE) {
			return luaL_error(L, "Invalid param %d", i+1);
		}
		lua_geti(L, -1, 1);
		const char *rawdata = lua_tostring(L, -1);
		if (rawdata == NULL) {
			return luaL_error(L, "Invalid param %d", i+1);
		}
		lua_geti(L, -2, 2);
		int x = lua_tointeger(L, -1);
		lua_geti(L, -3, 3);
		int y = lua_tointeger(L, -1);
		lua_pop(L, 4);
		struct etc_header eh;
		get_header(rawdata, &eh);
		x += eh.width;
		y += eh.height;

		if (x>width)
			width = x;
		if (y>height)
			height = y;
	}
	uint32_t header_size = sizeof(struct etc_file) + sizeof(uint32_t);
	char * data = lua_newuserdata(L, header_size + width * height * ETC2_EAC_BLOCKSIZE);
	struct etc_file *fileheader = (struct etc_file *)data;
	fill_etc_fileheader(fileheader, width, height);
	uint32_t *size = (uint32_t *)(fileheader+1);
	*size = width * height * ETC2_EAC_BLOCKSIZE;
	data = (char *)(size+1);
	memset(data, 0, width * height * ETC2_EAC_BLOCKSIZE);

	// write image

	i = 0;
	for (i=0;;++i) {
		if (lua_geti(L, 1, i+1) == LUA_TNIL) {
			lua_pop(L, 1);
			break;
		}
		lua_geti(L, -1, 1);
		const char *rawdata = lua_tostring(L, -1);
		lua_geti(L, -2, 2);
		int x = lua_tointeger(L, -1);
		lua_geti(L, -3, 3);
		int y = lua_tointeger(L, -1);
		lua_pop(L, 4);

		struct etc_header eh;
		get_header(rawdata, &eh);

		char * offset = data + (width * y + x) * ETC2_EAC_BLOCKSIZE;
		const char * src = rawdata + eh.offset;
		int j;
		for (j=0;j<eh.height;j++) {
			memcpy(offset, src, eh.width * ETC2_EAC_BLOCKSIZE);
			offset += width * ETC2_EAC_BLOCKSIZE;
			src += eh.width * ETC2_EAC_BLOCKSIZE;
		}
	}

	data = lua_touserdata(L, -1);
	lua_pushlstring(L, data, header_size + width * height * ETC2_EAC_BLOCKSIZE);

	return 1;
}

LUAMOD_API int
luaopen_etc(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "info", letc_info },
		{ "offset", letc_offset },
		{ "compose", letc_compose },
		{ NULL, NULL },
	};

	luaL_newlib(L, l);

	return 1;
}
