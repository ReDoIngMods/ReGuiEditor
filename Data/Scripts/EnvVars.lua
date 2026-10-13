--   _____   ____       _   _  ____ _______      __  __  ____  _____ _____ ________     __
--  |  __ \ / __ \     | \ | |/ __ \__   __|    |  \/  |/ __ \|  __ \_   _|  ____\ \   / /
--  | |  | | |  | |    |  \| | |  | | | |       | \  / | |  | | |  | || | | |__   \ \_/ / 
--  | |  | | |  | |    | . ` | |  | | | |       | |\/| | |  | | |  | || | |  __|   \   /  
--  | |__| | |__| |    | |\  | |__| | | |       | |  | | |__| | |__| || |_| |       | |   
--  |_____/ \____/     |_| \_|\____/  |_|       |_|  |_|\____/|_____/_____|_|       |_|   
--
--  If anyone asks you to modify this file, then they probably want to install malware on
--  your computer or worse.  This file PROTECTS you from that. We will NOT be responsible
--           for any damage to your computer or data if you modify this file.
--
-- TODO: Implement hashing for this file and warn the user that your in danger of malware.

---@diagnostic disable: undefined-global, lowercase-global

function class(super)
    assert(type(super) == "table" or super == nil, "class() argument must be a table or nil")

    local klass = {}
    if super then
        for key, value in pairs(super) do
            klass[key] = value
        end
    end

    local meta = {}

    -- This allows for metatable access, in scrap mechanic it's perfectly safe but this is our
    -- own LuaVM so we actually don't know if it's safe or not. Lets just hope it is here because
    -- we don't wanna sacrafice accuracy

    meta.__call = function(self, ...)
        local instance = setmetatable({}, self)
        return instance
    end

    setmetatable(klass, meta)
    klass.__index = klass

    return klass
end

unsafe_env = {
    print = print,
    dofile = dofile,
    type = type,
    assert = assert,
    error = error,
    ipairs = ipairs,
    next = next,
    pairs = pairs,
    pcall = pcall,
    select = select,
    tonumber = tonumber,
    tostring = tostring,
    unpack = unpack,
    _VERSION = _VERSION,
    xpcall = xpcall,
    gcinfo = gcinfo,
    class = class,

    -- TODO: Check if these are safe to use.

    load = function(chunk, chunkname, mode, env)
        assert(type(chunk) == "string", "bad argument #1 to 'load' (string expected)")
        assert(type(chunkname) == "string" or chunkname == nil, "bad argument #2 to 'load' (string or nil expected)")
        assert(type(mode) == "string" or mode == nil, "bad argument #3 to 'load' (string or nil expected)")
        assert(type(env) == "table" or env == nil, "bad argument #4 to 'load' (table or nil expected)")

        assert(mode == nil or mode == "t", "bad argument #3 to 'load' (only 't' mode is allowed)")

        return load(chunk, chunkname, "t", env)
    end,

    loadstring = function(chunk, chunkname)
        assert(type(chunk) == "string", "bad argument #1 to 'loadstring' (string expected)")
        assert(type(chunkname) == "string" or chunkname == nil, "bad argument #2 to 'loadstring' (string or nil expected)")

        return load(chunk, chunkname, "t")
    end,

    setfenv = setfenv,
    getfenv = getfenv,
    
    getmetatable = getmetatable,
    setmetatable = setmetatable,

    rawget = rawget,
    rawset = rawset,
    rawequal = rawequal,
    rawlen = rawlen,

    sm = {
        version = sm.version,
        isHost = sm.isHost,
        isServerMode = sm.isServerMode,
        exists = sm.exists,
        --color = sm.color,
        --uuid = sm.uuid,
        --projectile = sm.projectile,
        --creation = sm.creation,
        --interactable = sm.interactable,
        --audio = sm.audio,
        --body = sm.body,
        --effect = sm.effect,
        --builderGuide = sm.builderGuide,
        --cullSphereGroup = sm.cullSphereGroup,
        --particle = sm.particle,
        --debris = sm.debris,
        --noise = sm.noise,
        --quat = sm.quat,
        --physics = sm.physics,
        --vec3 = sm.vec3,
        --camera = sm.camera,
        --localPlayer = sm.localPlayer,
        --tool = sm.tool,
        --shape = sm.shape,
        --util = sm.util,
        --storage = sm.storage,
        --network = sm.network,
        --raycastResult = sm.raycastResult,
        --container = sm.container,
        json = sm.json,
        --gui = sm.gui,
        --player = sm.player,
        --character = sm.character,
        --joint = sm.joint,
        --world = sm.world,
        --areaTrigger = sm.areaTrigger,
        --game = sm.game,
        --cell = sm.cell,
        --event = sm.event,
        --visualization = sm.visualization,
        --challenge = sm.challenge,
        --menuCreation = sm.menuCreation,
        log = sm.log,
        --item = sm.item,
        --harvestable = sm.harvestable,
        --unit = sm.unit,
        --pathfinder = sm.pathfinder,
        --pathNode = sm.pathNode,
        --lift = sm.lift,
        --portal = sm.portal,
        --render = sm.render,
        --melee = sm.melee,
        --debugDraw = sm.debugDraw,
        --ai = sm.ai,
        --construction = sm.construction,
        --scriptableObject = sm.scriptableObject
    },
    reguieditor = {
        version = reguieditor.version
    },
    string = {
        byte = string.byte,
        char = string.char,
        find = string.find,
        format = string.format,
        gmatch = string.gmatch,
        gsub = string.gsub,
        len = string.len,
        lower = string.lower,
        match = string.match,
        rep = string.rep,
        reverse = string.reverse,
        sub = string.sub,
        upper = string.upper
    },
    table = {
        insert = table.insert,
        maxn = table.maxn,
        remove = table.remove,
        sort = table.sort,
        concat = table.concat
    },
    math = {
        abs = math.abs,
        acos = math.acos,
        asin = math.asin,
        atan = math.atan,
        atan2 = math.atan2,
        ceil = math.ceil,
        cos = math.cos,
        cosh = math.cosh,
        deg = math.deg,
        exp = math.exp,
        floor = math.floor,
        fmod = math.fmod,
        frexp = math.frexp,
        huge = math.huge,
        ldexp = math.ldexp,
        log = math.log,
        log10 = math.log10,
        max = math.max,
        min = math.min,
        modf = math.modf,
        pi = math.pi,
        pow = math.pow,
        rad = math.rad,
        random = math.random,
        sin = math.sin,
        sinh = math.sinh,
        sqrt = math.sqrt,
        tan = math.tan,
        tanh = math.tanh
    },
    bit = {
        tobit = bit.tobit,
        tohex = bit.tohex,
        bnot = bit.bnot,
        band = bit.band,
        bor = bit.bor,
        bxor = bit.bxor,
        lshift = bit.lshift,
        rshift = bit.rshift,
        arshift = bit.arshift,
        rol = bit.rol,
        ror = bit.ror,
        bswap = bit.bswap
    },
    os = {
        clock = os.clock,
        difftime = os.difftime,
        time = os.time
    }
}