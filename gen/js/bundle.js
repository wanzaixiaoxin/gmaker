/*eslint-disable block-scoped-var, id-length, no-control-regex, no-magic-numbers, no-prototype-builtins, no-redeclare, no-shadow, no-var, sort-vars*/
"use strict";

var $protobuf = require("protobufjs/minimal");

// Common aliases
var $Reader = $protobuf.Reader, $Writer = $protobuf.Writer, $util = $protobuf.util;

// Exported root namespace
var $root = $protobuf.roots["default"] || ($protobuf.roots["default"] = {});

$root.biz = (function() {

    /**
     * Namespace biz.
     * @exports biz
     * @namespace
     */
    var biz = {};

    biz.PlayerBase = (function() {

        /**
         * Properties of a PlayerBase.
         * @memberof biz
         * @interface IPlayerBase
         * @property {number|Long|null} [playerId] PlayerBase playerId
         * @property {string|null} [nickname] PlayerBase nickname
         * @property {number|null} [level] PlayerBase level
         * @property {number|Long|null} [exp] PlayerBase exp
         * @property {number|Long|null} [coin] PlayerBase coin
         * @property {number|Long|null} [diamond] PlayerBase diamond
         * @property {number|Long|null} [createAt] PlayerBase createAt
         * @property {number|Long|null} [loginAt] PlayerBase loginAt
         */

        /**
         * Constructs a new PlayerBase.
         * @memberof biz
         * @classdesc Represents a PlayerBase.
         * @implements IPlayerBase
         * @constructor
         * @param {biz.IPlayerBase=} [properties] Properties to set
         */
        function PlayerBase(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * PlayerBase playerId.
         * @member {number|Long} playerId
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * PlayerBase nickname.
         * @member {string} nickname
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.nickname = "";

        /**
         * PlayerBase level.
         * @member {number} level
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.level = 0;

        /**
         * PlayerBase exp.
         * @member {number|Long} exp
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.exp = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * PlayerBase coin.
         * @member {number|Long} coin
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.coin = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * PlayerBase diamond.
         * @member {number|Long} diamond
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.diamond = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * PlayerBase createAt.
         * @member {number|Long} createAt
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.createAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * PlayerBase loginAt.
         * @member {number|Long} loginAt
         * @memberof biz.PlayerBase
         * @instance
         */
        PlayerBase.prototype.loginAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new PlayerBase instance using the specified properties.
         * @function create
         * @memberof biz.PlayerBase
         * @static
         * @param {biz.IPlayerBase=} [properties] Properties to set
         * @returns {biz.PlayerBase} PlayerBase instance
         */
        PlayerBase.create = function create(properties) {
            return new PlayerBase(properties);
        };

        /**
         * Encodes the specified PlayerBase message. Does not implicitly {@link biz.PlayerBase.verify|verify} messages.
         * @function encode
         * @memberof biz.PlayerBase
         * @static
         * @param {biz.IPlayerBase} message PlayerBase message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PlayerBase.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.playerId);
            if (message.nickname != null && Object.hasOwnProperty.call(message, "nickname"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.nickname);
            if (message.level != null && Object.hasOwnProperty.call(message, "level"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.level);
            if (message.exp != null && Object.hasOwnProperty.call(message, "exp"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint64(message.exp);
            if (message.coin != null && Object.hasOwnProperty.call(message, "coin"))
                writer.uint32(/* id 5, wireType 0 =*/40).uint64(message.coin);
            if (message.diamond != null && Object.hasOwnProperty.call(message, "diamond"))
                writer.uint32(/* id 6, wireType 0 =*/48).uint64(message.diamond);
            if (message.createAt != null && Object.hasOwnProperty.call(message, "createAt"))
                writer.uint32(/* id 7, wireType 0 =*/56).uint64(message.createAt);
            if (message.loginAt != null && Object.hasOwnProperty.call(message, "loginAt"))
                writer.uint32(/* id 8, wireType 0 =*/64).uint64(message.loginAt);
            return writer;
        };

        /**
         * Encodes the specified PlayerBase message, length delimited. Does not implicitly {@link biz.PlayerBase.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.PlayerBase
         * @static
         * @param {biz.IPlayerBase} message PlayerBase message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PlayerBase.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a PlayerBase message from the specified reader or buffer.
         * @function decode
         * @memberof biz.PlayerBase
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.PlayerBase} PlayerBase
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PlayerBase.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.PlayerBase();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.playerId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.nickname = reader.string();
                        break;
                    }
                case 3: {
                        message.level = reader.uint32();
                        break;
                    }
                case 4: {
                        message.exp = reader.uint64();
                        break;
                    }
                case 5: {
                        message.coin = reader.uint64();
                        break;
                    }
                case 6: {
                        message.diamond = reader.uint64();
                        break;
                    }
                case 7: {
                        message.createAt = reader.uint64();
                        break;
                    }
                case 8: {
                        message.loginAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a PlayerBase message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.PlayerBase
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.PlayerBase} PlayerBase
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PlayerBase.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a PlayerBase message.
         * @function verify
         * @memberof biz.PlayerBase
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        PlayerBase.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            if (message.nickname != null && message.hasOwnProperty("nickname"))
                if (!$util.isString(message.nickname))
                    return "nickname: string expected";
            if (message.level != null && message.hasOwnProperty("level"))
                if (!$util.isInteger(message.level))
                    return "level: integer expected";
            if (message.exp != null && message.hasOwnProperty("exp"))
                if (!$util.isInteger(message.exp) && !(message.exp && $util.isInteger(message.exp.low) && $util.isInteger(message.exp.high)))
                    return "exp: integer|Long expected";
            if (message.coin != null && message.hasOwnProperty("coin"))
                if (!$util.isInteger(message.coin) && !(message.coin && $util.isInteger(message.coin.low) && $util.isInteger(message.coin.high)))
                    return "coin: integer|Long expected";
            if (message.diamond != null && message.hasOwnProperty("diamond"))
                if (!$util.isInteger(message.diamond) && !(message.diamond && $util.isInteger(message.diamond.low) && $util.isInteger(message.diamond.high)))
                    return "diamond: integer|Long expected";
            if (message.createAt != null && message.hasOwnProperty("createAt"))
                if (!$util.isInteger(message.createAt) && !(message.createAt && $util.isInteger(message.createAt.low) && $util.isInteger(message.createAt.high)))
                    return "createAt: integer|Long expected";
            if (message.loginAt != null && message.hasOwnProperty("loginAt"))
                if (!$util.isInteger(message.loginAt) && !(message.loginAt && $util.isInteger(message.loginAt.low) && $util.isInteger(message.loginAt.high)))
                    return "loginAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a PlayerBase message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.PlayerBase
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.PlayerBase} PlayerBase
         */
        PlayerBase.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.PlayerBase)
                return object;
            var message = new $root.biz.PlayerBase();
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            if (object.nickname != null)
                message.nickname = String(object.nickname);
            if (object.level != null)
                message.level = object.level >>> 0;
            if (object.exp != null)
                if ($util.Long)
                    (message.exp = $util.Long.fromValue(object.exp)).unsigned = true;
                else if (typeof object.exp === "string")
                    message.exp = parseInt(object.exp, 10);
                else if (typeof object.exp === "number")
                    message.exp = object.exp;
                else if (typeof object.exp === "object")
                    message.exp = new $util.LongBits(object.exp.low >>> 0, object.exp.high >>> 0).toNumber(true);
            if (object.coin != null)
                if ($util.Long)
                    (message.coin = $util.Long.fromValue(object.coin)).unsigned = true;
                else if (typeof object.coin === "string")
                    message.coin = parseInt(object.coin, 10);
                else if (typeof object.coin === "number")
                    message.coin = object.coin;
                else if (typeof object.coin === "object")
                    message.coin = new $util.LongBits(object.coin.low >>> 0, object.coin.high >>> 0).toNumber(true);
            if (object.diamond != null)
                if ($util.Long)
                    (message.diamond = $util.Long.fromValue(object.diamond)).unsigned = true;
                else if (typeof object.diamond === "string")
                    message.diamond = parseInt(object.diamond, 10);
                else if (typeof object.diamond === "number")
                    message.diamond = object.diamond;
                else if (typeof object.diamond === "object")
                    message.diamond = new $util.LongBits(object.diamond.low >>> 0, object.diamond.high >>> 0).toNumber(true);
            if (object.createAt != null)
                if ($util.Long)
                    (message.createAt = $util.Long.fromValue(object.createAt)).unsigned = true;
                else if (typeof object.createAt === "string")
                    message.createAt = parseInt(object.createAt, 10);
                else if (typeof object.createAt === "number")
                    message.createAt = object.createAt;
                else if (typeof object.createAt === "object")
                    message.createAt = new $util.LongBits(object.createAt.low >>> 0, object.createAt.high >>> 0).toNumber(true);
            if (object.loginAt != null)
                if ($util.Long)
                    (message.loginAt = $util.Long.fromValue(object.loginAt)).unsigned = true;
                else if (typeof object.loginAt === "string")
                    message.loginAt = parseInt(object.loginAt, 10);
                else if (typeof object.loginAt === "number")
                    message.loginAt = object.loginAt;
                else if (typeof object.loginAt === "object")
                    message.loginAt = new $util.LongBits(object.loginAt.low >>> 0, object.loginAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a PlayerBase message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.PlayerBase
         * @static
         * @param {biz.PlayerBase} message PlayerBase
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        PlayerBase.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
                object.nickname = "";
                object.level = 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.exp = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.exp = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.coin = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.coin = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.diamond = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.diamond = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.createAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.createAt = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.loginAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.loginAt = options.longs === String ? "0" : 0;
            }
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            if (message.nickname != null && message.hasOwnProperty("nickname"))
                object.nickname = message.nickname;
            if (message.level != null && message.hasOwnProperty("level"))
                object.level = message.level;
            if (message.exp != null && message.hasOwnProperty("exp"))
                if (typeof message.exp === "number")
                    object.exp = options.longs === String ? String(message.exp) : message.exp;
                else
                    object.exp = options.longs === String ? $util.Long.prototype.toString.call(message.exp) : options.longs === Number ? new $util.LongBits(message.exp.low >>> 0, message.exp.high >>> 0).toNumber(true) : message.exp;
            if (message.coin != null && message.hasOwnProperty("coin"))
                if (typeof message.coin === "number")
                    object.coin = options.longs === String ? String(message.coin) : message.coin;
                else
                    object.coin = options.longs === String ? $util.Long.prototype.toString.call(message.coin) : options.longs === Number ? new $util.LongBits(message.coin.low >>> 0, message.coin.high >>> 0).toNumber(true) : message.coin;
            if (message.diamond != null && message.hasOwnProperty("diamond"))
                if (typeof message.diamond === "number")
                    object.diamond = options.longs === String ? String(message.diamond) : message.diamond;
                else
                    object.diamond = options.longs === String ? $util.Long.prototype.toString.call(message.diamond) : options.longs === Number ? new $util.LongBits(message.diamond.low >>> 0, message.diamond.high >>> 0).toNumber(true) : message.diamond;
            if (message.createAt != null && message.hasOwnProperty("createAt"))
                if (typeof message.createAt === "number")
                    object.createAt = options.longs === String ? String(message.createAt) : message.createAt;
                else
                    object.createAt = options.longs === String ? $util.Long.prototype.toString.call(message.createAt) : options.longs === Number ? new $util.LongBits(message.createAt.low >>> 0, message.createAt.high >>> 0).toNumber(true) : message.createAt;
            if (message.loginAt != null && message.hasOwnProperty("loginAt"))
                if (typeof message.loginAt === "number")
                    object.loginAt = options.longs === String ? String(message.loginAt) : message.loginAt;
                else
                    object.loginAt = options.longs === String ? $util.Long.prototype.toString.call(message.loginAt) : options.longs === Number ? new $util.LongBits(message.loginAt.low >>> 0, message.loginAt.high >>> 0).toNumber(true) : message.loginAt;
            return object;
        };

        /**
         * Converts this PlayerBase to JSON.
         * @function toJSON
         * @memberof biz.PlayerBase
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        PlayerBase.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for PlayerBase
         * @function getTypeUrl
         * @memberof biz.PlayerBase
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        PlayerBase.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.PlayerBase";
        };

        return PlayerBase;
    })();

    biz.GetPlayerReq = (function() {

        /**
         * Properties of a GetPlayerReq.
         * @memberof biz
         * @interface IGetPlayerReq
         * @property {number|Long|null} [playerId] GetPlayerReq playerId
         */

        /**
         * Constructs a new GetPlayerReq.
         * @memberof biz
         * @classdesc Represents a GetPlayerReq.
         * @implements IGetPlayerReq
         * @constructor
         * @param {biz.IGetPlayerReq=} [properties] Properties to set
         */
        function GetPlayerReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetPlayerReq playerId.
         * @member {number|Long} playerId
         * @memberof biz.GetPlayerReq
         * @instance
         */
        GetPlayerReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new GetPlayerReq instance using the specified properties.
         * @function create
         * @memberof biz.GetPlayerReq
         * @static
         * @param {biz.IGetPlayerReq=} [properties] Properties to set
         * @returns {biz.GetPlayerReq} GetPlayerReq instance
         */
        GetPlayerReq.create = function create(properties) {
            return new GetPlayerReq(properties);
        };

        /**
         * Encodes the specified GetPlayerReq message. Does not implicitly {@link biz.GetPlayerReq.verify|verify} messages.
         * @function encode
         * @memberof biz.GetPlayerReq
         * @static
         * @param {biz.IGetPlayerReq} message GetPlayerReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified GetPlayerReq message, length delimited. Does not implicitly {@link biz.GetPlayerReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetPlayerReq
         * @static
         * @param {biz.IGetPlayerReq} message GetPlayerReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetPlayerReq message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetPlayerReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetPlayerReq} GetPlayerReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetPlayerReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetPlayerReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetPlayerReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetPlayerReq} GetPlayerReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetPlayerReq message.
         * @function verify
         * @memberof biz.GetPlayerReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetPlayerReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a GetPlayerReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetPlayerReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetPlayerReq} GetPlayerReq
         */
        GetPlayerReq.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetPlayerReq)
                return object;
            var message = new $root.biz.GetPlayerReq();
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a GetPlayerReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetPlayerReq
         * @static
         * @param {biz.GetPlayerReq} message GetPlayerReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetPlayerReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this GetPlayerReq to JSON.
         * @function toJSON
         * @memberof biz.GetPlayerReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetPlayerReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetPlayerReq
         * @function getTypeUrl
         * @memberof biz.GetPlayerReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetPlayerReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetPlayerReq";
        };

        return GetPlayerReq;
    })();

    biz.GetPlayerRes = (function() {

        /**
         * Properties of a GetPlayerRes.
         * @memberof biz
         * @interface IGetPlayerRes
         * @property {common.IResult|null} [result] GetPlayerRes result
         * @property {biz.IPlayerBase|null} [player] GetPlayerRes player
         */

        /**
         * Constructs a new GetPlayerRes.
         * @memberof biz
         * @classdesc Represents a GetPlayerRes.
         * @implements IGetPlayerRes
         * @constructor
         * @param {biz.IGetPlayerRes=} [properties] Properties to set
         */
        function GetPlayerRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetPlayerRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof biz.GetPlayerRes
         * @instance
         */
        GetPlayerRes.prototype.result = null;

        /**
         * GetPlayerRes player.
         * @member {biz.IPlayerBase|null|undefined} player
         * @memberof biz.GetPlayerRes
         * @instance
         */
        GetPlayerRes.prototype.player = null;

        /**
         * Creates a new GetPlayerRes instance using the specified properties.
         * @function create
         * @memberof biz.GetPlayerRes
         * @static
         * @param {biz.IGetPlayerRes=} [properties] Properties to set
         * @returns {biz.GetPlayerRes} GetPlayerRes instance
         */
        GetPlayerRes.create = function create(properties) {
            return new GetPlayerRes(properties);
        };

        /**
         * Encodes the specified GetPlayerRes message. Does not implicitly {@link biz.GetPlayerRes.verify|verify} messages.
         * @function encode
         * @memberof biz.GetPlayerRes
         * @static
         * @param {biz.IGetPlayerRes} message GetPlayerRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.player != null && Object.hasOwnProperty.call(message, "player"))
                $root.biz.PlayerBase.encode(message.player, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified GetPlayerRes message, length delimited. Does not implicitly {@link biz.GetPlayerRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetPlayerRes
         * @static
         * @param {biz.IGetPlayerRes} message GetPlayerRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetPlayerRes message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetPlayerRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetPlayerRes} GetPlayerRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetPlayerRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.player = $root.biz.PlayerBase.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetPlayerRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetPlayerRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetPlayerRes} GetPlayerRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetPlayerRes message.
         * @function verify
         * @memberof biz.GetPlayerRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetPlayerRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.player != null && message.hasOwnProperty("player")) {
                var error = $root.biz.PlayerBase.verify(message.player);
                if (error)
                    return "player." + error;
            }
            return null;
        };

        /**
         * Creates a GetPlayerRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetPlayerRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetPlayerRes} GetPlayerRes
         */
        GetPlayerRes.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetPlayerRes)
                return object;
            var message = new $root.biz.GetPlayerRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".biz.GetPlayerRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.player != null) {
                if (typeof object.player !== "object")
                    throw TypeError(".biz.GetPlayerRes.player: object expected");
                message.player = $root.biz.PlayerBase.fromObject(object.player);
            }
            return message;
        };

        /**
         * Creates a plain object from a GetPlayerRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetPlayerRes
         * @static
         * @param {biz.GetPlayerRes} message GetPlayerRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetPlayerRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                object.player = null;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.player != null && message.hasOwnProperty("player"))
                object.player = $root.biz.PlayerBase.toObject(message.player, options);
            return object;
        };

        /**
         * Converts this GetPlayerRes to JSON.
         * @function toJSON
         * @memberof biz.GetPlayerRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetPlayerRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetPlayerRes
         * @function getTypeUrl
         * @memberof biz.GetPlayerRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetPlayerRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetPlayerRes";
        };

        return GetPlayerRes;
    })();

    biz.Item = (function() {

        /**
         * Properties of an Item.
         * @memberof biz
         * @interface IItem
         * @property {number|null} [itemId] Item itemId
         * @property {number|Long|null} [count] Item count
         */

        /**
         * Constructs a new Item.
         * @memberof biz
         * @classdesc Represents an Item.
         * @implements IItem
         * @constructor
         * @param {biz.IItem=} [properties] Properties to set
         */
        function Item(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Item itemId.
         * @member {number} itemId
         * @memberof biz.Item
         * @instance
         */
        Item.prototype.itemId = 0;

        /**
         * Item count.
         * @member {number|Long} count
         * @memberof biz.Item
         * @instance
         */
        Item.prototype.count = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new Item instance using the specified properties.
         * @function create
         * @memberof biz.Item
         * @static
         * @param {biz.IItem=} [properties] Properties to set
         * @returns {biz.Item} Item instance
         */
        Item.create = function create(properties) {
            return new Item(properties);
        };

        /**
         * Encodes the specified Item message. Does not implicitly {@link biz.Item.verify|verify} messages.
         * @function encode
         * @memberof biz.Item
         * @static
         * @param {biz.IItem} message Item message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Item.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.itemId != null && Object.hasOwnProperty.call(message, "itemId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.itemId);
            if (message.count != null && Object.hasOwnProperty.call(message, "count"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.count);
            return writer;
        };

        /**
         * Encodes the specified Item message, length delimited. Does not implicitly {@link biz.Item.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.Item
         * @static
         * @param {biz.IItem} message Item message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Item.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes an Item message from the specified reader or buffer.
         * @function decode
         * @memberof biz.Item
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.Item} Item
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Item.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.Item();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.itemId = reader.uint32();
                        break;
                    }
                case 2: {
                        message.count = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes an Item message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.Item
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.Item} Item
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Item.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies an Item message.
         * @function verify
         * @memberof biz.Item
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Item.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.itemId != null && message.hasOwnProperty("itemId"))
                if (!$util.isInteger(message.itemId))
                    return "itemId: integer expected";
            if (message.count != null && message.hasOwnProperty("count"))
                if (!$util.isInteger(message.count) && !(message.count && $util.isInteger(message.count.low) && $util.isInteger(message.count.high)))
                    return "count: integer|Long expected";
            return null;
        };

        /**
         * Creates an Item message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.Item
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.Item} Item
         */
        Item.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.Item)
                return object;
            var message = new $root.biz.Item();
            if (object.itemId != null)
                message.itemId = object.itemId >>> 0;
            if (object.count != null)
                if ($util.Long)
                    (message.count = $util.Long.fromValue(object.count)).unsigned = true;
                else if (typeof object.count === "string")
                    message.count = parseInt(object.count, 10);
                else if (typeof object.count === "number")
                    message.count = object.count;
                else if (typeof object.count === "object")
                    message.count = new $util.LongBits(object.count.low >>> 0, object.count.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from an Item message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.Item
         * @static
         * @param {biz.Item} message Item
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Item.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.itemId = 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.count = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.count = options.longs === String ? "0" : 0;
            }
            if (message.itemId != null && message.hasOwnProperty("itemId"))
                object.itemId = message.itemId;
            if (message.count != null && message.hasOwnProperty("count"))
                if (typeof message.count === "number")
                    object.count = options.longs === String ? String(message.count) : message.count;
                else
                    object.count = options.longs === String ? $util.Long.prototype.toString.call(message.count) : options.longs === Number ? new $util.LongBits(message.count.low >>> 0, message.count.high >>> 0).toNumber(true) : message.count;
            return object;
        };

        /**
         * Converts this Item to JSON.
         * @function toJSON
         * @memberof biz.Item
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Item.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Item
         * @function getTypeUrl
         * @memberof biz.Item
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Item.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.Item";
        };

        return Item;
    })();

    biz.GetBagReq = (function() {

        /**
         * Properties of a GetBagReq.
         * @memberof biz
         * @interface IGetBagReq
         * @property {number|Long|null} [playerId] GetBagReq playerId
         */

        /**
         * Constructs a new GetBagReq.
         * @memberof biz
         * @classdesc Represents a GetBagReq.
         * @implements IGetBagReq
         * @constructor
         * @param {biz.IGetBagReq=} [properties] Properties to set
         */
        function GetBagReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetBagReq playerId.
         * @member {number|Long} playerId
         * @memberof biz.GetBagReq
         * @instance
         */
        GetBagReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new GetBagReq instance using the specified properties.
         * @function create
         * @memberof biz.GetBagReq
         * @static
         * @param {biz.IGetBagReq=} [properties] Properties to set
         * @returns {biz.GetBagReq} GetBagReq instance
         */
        GetBagReq.create = function create(properties) {
            return new GetBagReq(properties);
        };

        /**
         * Encodes the specified GetBagReq message. Does not implicitly {@link biz.GetBagReq.verify|verify} messages.
         * @function encode
         * @memberof biz.GetBagReq
         * @static
         * @param {biz.IGetBagReq} message GetBagReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetBagReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified GetBagReq message, length delimited. Does not implicitly {@link biz.GetBagReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetBagReq
         * @static
         * @param {biz.IGetBagReq} message GetBagReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetBagReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetBagReq message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetBagReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetBagReq} GetBagReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetBagReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetBagReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetBagReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetBagReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetBagReq} GetBagReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetBagReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetBagReq message.
         * @function verify
         * @memberof biz.GetBagReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetBagReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a GetBagReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetBagReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetBagReq} GetBagReq
         */
        GetBagReq.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetBagReq)
                return object;
            var message = new $root.biz.GetBagReq();
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a GetBagReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetBagReq
         * @static
         * @param {biz.GetBagReq} message GetBagReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetBagReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this GetBagReq to JSON.
         * @function toJSON
         * @memberof biz.GetBagReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetBagReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetBagReq
         * @function getTypeUrl
         * @memberof biz.GetBagReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetBagReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetBagReq";
        };

        return GetBagReq;
    })();

    biz.GetBagRes = (function() {

        /**
         * Properties of a GetBagRes.
         * @memberof biz
         * @interface IGetBagRes
         * @property {common.IResult|null} [result] GetBagRes result
         * @property {Array.<biz.IItem>|null} [items] GetBagRes items
         */

        /**
         * Constructs a new GetBagRes.
         * @memberof biz
         * @classdesc Represents a GetBagRes.
         * @implements IGetBagRes
         * @constructor
         * @param {biz.IGetBagRes=} [properties] Properties to set
         */
        function GetBagRes(properties) {
            this.items = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetBagRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof biz.GetBagRes
         * @instance
         */
        GetBagRes.prototype.result = null;

        /**
         * GetBagRes items.
         * @member {Array.<biz.IItem>} items
         * @memberof biz.GetBagRes
         * @instance
         */
        GetBagRes.prototype.items = $util.emptyArray;

        /**
         * Creates a new GetBagRes instance using the specified properties.
         * @function create
         * @memberof biz.GetBagRes
         * @static
         * @param {biz.IGetBagRes=} [properties] Properties to set
         * @returns {biz.GetBagRes} GetBagRes instance
         */
        GetBagRes.create = function create(properties) {
            return new GetBagRes(properties);
        };

        /**
         * Encodes the specified GetBagRes message. Does not implicitly {@link biz.GetBagRes.verify|verify} messages.
         * @function encode
         * @memberof biz.GetBagRes
         * @static
         * @param {biz.IGetBagRes} message GetBagRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetBagRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.items != null && message.items.length)
                for (var i = 0; i < message.items.length; ++i)
                    $root.biz.Item.encode(message.items[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified GetBagRes message, length delimited. Does not implicitly {@link biz.GetBagRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetBagRes
         * @static
         * @param {biz.IGetBagRes} message GetBagRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetBagRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetBagRes message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetBagRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetBagRes} GetBagRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetBagRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetBagRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        if (!(message.items && message.items.length))
                            message.items = [];
                        message.items.push($root.biz.Item.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetBagRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetBagRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetBagRes} GetBagRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetBagRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetBagRes message.
         * @function verify
         * @memberof biz.GetBagRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetBagRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.items != null && message.hasOwnProperty("items")) {
                if (!Array.isArray(message.items))
                    return "items: array expected";
                for (var i = 0; i < message.items.length; ++i) {
                    var error = $root.biz.Item.verify(message.items[i]);
                    if (error)
                        return "items." + error;
                }
            }
            return null;
        };

        /**
         * Creates a GetBagRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetBagRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetBagRes} GetBagRes
         */
        GetBagRes.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetBagRes)
                return object;
            var message = new $root.biz.GetBagRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".biz.GetBagRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.items) {
                if (!Array.isArray(object.items))
                    throw TypeError(".biz.GetBagRes.items: array expected");
                message.items = [];
                for (var i = 0; i < object.items.length; ++i) {
                    if (typeof object.items[i] !== "object")
                        throw TypeError(".biz.GetBagRes.items: object expected");
                    message.items[i] = $root.biz.Item.fromObject(object.items[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a GetBagRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetBagRes
         * @static
         * @param {biz.GetBagRes} message GetBagRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetBagRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.items = [];
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.items && message.items.length) {
                object.items = [];
                for (var j = 0; j < message.items.length; ++j)
                    object.items[j] = $root.biz.Item.toObject(message.items[j], options);
            }
            return object;
        };

        /**
         * Converts this GetBagRes to JSON.
         * @function toJSON
         * @memberof biz.GetBagRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetBagRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetBagRes
         * @function getTypeUrl
         * @memberof biz.GetBagRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetBagRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetBagRes";
        };

        return GetBagRes;
    })();

    biz.UpdatePlayerReq = (function() {

        /**
         * Properties of an UpdatePlayerReq.
         * @memberof biz
         * @interface IUpdatePlayerReq
         * @property {number|Long|null} [playerId] UpdatePlayerReq playerId
         * @property {string|null} [nickname] UpdatePlayerReq nickname
         * @property {number|Long|null} [coin] UpdatePlayerReq coin
         * @property {number|Long|null} [diamond] UpdatePlayerReq diamond
         */

        /**
         * Constructs a new UpdatePlayerReq.
         * @memberof biz
         * @classdesc Represents an UpdatePlayerReq.
         * @implements IUpdatePlayerReq
         * @constructor
         * @param {biz.IUpdatePlayerReq=} [properties] Properties to set
         */
        function UpdatePlayerReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * UpdatePlayerReq playerId.
         * @member {number|Long} playerId
         * @memberof biz.UpdatePlayerReq
         * @instance
         */
        UpdatePlayerReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * UpdatePlayerReq nickname.
         * @member {string} nickname
         * @memberof biz.UpdatePlayerReq
         * @instance
         */
        UpdatePlayerReq.prototype.nickname = "";

        /**
         * UpdatePlayerReq coin.
         * @member {number|Long} coin
         * @memberof biz.UpdatePlayerReq
         * @instance
         */
        UpdatePlayerReq.prototype.coin = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * UpdatePlayerReq diamond.
         * @member {number|Long} diamond
         * @memberof biz.UpdatePlayerReq
         * @instance
         */
        UpdatePlayerReq.prototype.diamond = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new UpdatePlayerReq instance using the specified properties.
         * @function create
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {biz.IUpdatePlayerReq=} [properties] Properties to set
         * @returns {biz.UpdatePlayerReq} UpdatePlayerReq instance
         */
        UpdatePlayerReq.create = function create(properties) {
            return new UpdatePlayerReq(properties);
        };

        /**
         * Encodes the specified UpdatePlayerReq message. Does not implicitly {@link biz.UpdatePlayerReq.verify|verify} messages.
         * @function encode
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {biz.IUpdatePlayerReq} message UpdatePlayerReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        UpdatePlayerReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.playerId);
            if (message.nickname != null && Object.hasOwnProperty.call(message, "nickname"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.nickname);
            if (message.coin != null && Object.hasOwnProperty.call(message, "coin"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint64(message.coin);
            if (message.diamond != null && Object.hasOwnProperty.call(message, "diamond"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint64(message.diamond);
            return writer;
        };

        /**
         * Encodes the specified UpdatePlayerReq message, length delimited. Does not implicitly {@link biz.UpdatePlayerReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {biz.IUpdatePlayerReq} message UpdatePlayerReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        UpdatePlayerReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes an UpdatePlayerReq message from the specified reader or buffer.
         * @function decode
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.UpdatePlayerReq} UpdatePlayerReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        UpdatePlayerReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.UpdatePlayerReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.playerId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.nickname = reader.string();
                        break;
                    }
                case 3: {
                        message.coin = reader.uint64();
                        break;
                    }
                case 4: {
                        message.diamond = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes an UpdatePlayerReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.UpdatePlayerReq} UpdatePlayerReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        UpdatePlayerReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies an UpdatePlayerReq message.
         * @function verify
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        UpdatePlayerReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            if (message.nickname != null && message.hasOwnProperty("nickname"))
                if (!$util.isString(message.nickname))
                    return "nickname: string expected";
            if (message.coin != null && message.hasOwnProperty("coin"))
                if (!$util.isInteger(message.coin) && !(message.coin && $util.isInteger(message.coin.low) && $util.isInteger(message.coin.high)))
                    return "coin: integer|Long expected";
            if (message.diamond != null && message.hasOwnProperty("diamond"))
                if (!$util.isInteger(message.diamond) && !(message.diamond && $util.isInteger(message.diamond.low) && $util.isInteger(message.diamond.high)))
                    return "diamond: integer|Long expected";
            return null;
        };

        /**
         * Creates an UpdatePlayerReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.UpdatePlayerReq} UpdatePlayerReq
         */
        UpdatePlayerReq.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.UpdatePlayerReq)
                return object;
            var message = new $root.biz.UpdatePlayerReq();
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            if (object.nickname != null)
                message.nickname = String(object.nickname);
            if (object.coin != null)
                if ($util.Long)
                    (message.coin = $util.Long.fromValue(object.coin)).unsigned = true;
                else if (typeof object.coin === "string")
                    message.coin = parseInt(object.coin, 10);
                else if (typeof object.coin === "number")
                    message.coin = object.coin;
                else if (typeof object.coin === "object")
                    message.coin = new $util.LongBits(object.coin.low >>> 0, object.coin.high >>> 0).toNumber(true);
            if (object.diamond != null)
                if ($util.Long)
                    (message.diamond = $util.Long.fromValue(object.diamond)).unsigned = true;
                else if (typeof object.diamond === "string")
                    message.diamond = parseInt(object.diamond, 10);
                else if (typeof object.diamond === "number")
                    message.diamond = object.diamond;
                else if (typeof object.diamond === "object")
                    message.diamond = new $util.LongBits(object.diamond.low >>> 0, object.diamond.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from an UpdatePlayerReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {biz.UpdatePlayerReq} message UpdatePlayerReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        UpdatePlayerReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
                object.nickname = "";
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.coin = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.coin = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.diamond = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.diamond = options.longs === String ? "0" : 0;
            }
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            if (message.nickname != null && message.hasOwnProperty("nickname"))
                object.nickname = message.nickname;
            if (message.coin != null && message.hasOwnProperty("coin"))
                if (typeof message.coin === "number")
                    object.coin = options.longs === String ? String(message.coin) : message.coin;
                else
                    object.coin = options.longs === String ? $util.Long.prototype.toString.call(message.coin) : options.longs === Number ? new $util.LongBits(message.coin.low >>> 0, message.coin.high >>> 0).toNumber(true) : message.coin;
            if (message.diamond != null && message.hasOwnProperty("diamond"))
                if (typeof message.diamond === "number")
                    object.diamond = options.longs === String ? String(message.diamond) : message.diamond;
                else
                    object.diamond = options.longs === String ? $util.Long.prototype.toString.call(message.diamond) : options.longs === Number ? new $util.LongBits(message.diamond.low >>> 0, message.diamond.high >>> 0).toNumber(true) : message.diamond;
            return object;
        };

        /**
         * Converts this UpdatePlayerReq to JSON.
         * @function toJSON
         * @memberof biz.UpdatePlayerReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        UpdatePlayerReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for UpdatePlayerReq
         * @function getTypeUrl
         * @memberof biz.UpdatePlayerReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        UpdatePlayerReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.UpdatePlayerReq";
        };

        return UpdatePlayerReq;
    })();

    biz.UpdatePlayerRes = (function() {

        /**
         * Properties of an UpdatePlayerRes.
         * @memberof biz
         * @interface IUpdatePlayerRes
         * @property {common.IResult|null} [result] UpdatePlayerRes result
         */

        /**
         * Constructs a new UpdatePlayerRes.
         * @memberof biz
         * @classdesc Represents an UpdatePlayerRes.
         * @implements IUpdatePlayerRes
         * @constructor
         * @param {biz.IUpdatePlayerRes=} [properties] Properties to set
         */
        function UpdatePlayerRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * UpdatePlayerRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof biz.UpdatePlayerRes
         * @instance
         */
        UpdatePlayerRes.prototype.result = null;

        /**
         * Creates a new UpdatePlayerRes instance using the specified properties.
         * @function create
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {biz.IUpdatePlayerRes=} [properties] Properties to set
         * @returns {biz.UpdatePlayerRes} UpdatePlayerRes instance
         */
        UpdatePlayerRes.create = function create(properties) {
            return new UpdatePlayerRes(properties);
        };

        /**
         * Encodes the specified UpdatePlayerRes message. Does not implicitly {@link biz.UpdatePlayerRes.verify|verify} messages.
         * @function encode
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {biz.IUpdatePlayerRes} message UpdatePlayerRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        UpdatePlayerRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified UpdatePlayerRes message, length delimited. Does not implicitly {@link biz.UpdatePlayerRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {biz.IUpdatePlayerRes} message UpdatePlayerRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        UpdatePlayerRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes an UpdatePlayerRes message from the specified reader or buffer.
         * @function decode
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.UpdatePlayerRes} UpdatePlayerRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        UpdatePlayerRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.UpdatePlayerRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes an UpdatePlayerRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.UpdatePlayerRes} UpdatePlayerRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        UpdatePlayerRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies an UpdatePlayerRes message.
         * @function verify
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        UpdatePlayerRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            return null;
        };

        /**
         * Creates an UpdatePlayerRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.UpdatePlayerRes} UpdatePlayerRes
         */
        UpdatePlayerRes.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.UpdatePlayerRes)
                return object;
            var message = new $root.biz.UpdatePlayerRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".biz.UpdatePlayerRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            return message;
        };

        /**
         * Creates a plain object from an UpdatePlayerRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {biz.UpdatePlayerRes} message UpdatePlayerRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        UpdatePlayerRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            return object;
        };

        /**
         * Converts this UpdatePlayerRes to JSON.
         * @function toJSON
         * @memberof biz.UpdatePlayerRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        UpdatePlayerRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for UpdatePlayerRes
         * @function getTypeUrl
         * @memberof biz.UpdatePlayerRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        UpdatePlayerRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.UpdatePlayerRes";
        };

        return UpdatePlayerRes;
    })();

    biz.GetPlayerRoomsReq = (function() {

        /**
         * Properties of a GetPlayerRoomsReq.
         * @memberof biz
         * @interface IGetPlayerRoomsReq
         * @property {number|Long|null} [playerId] GetPlayerRoomsReq playerId
         */

        /**
         * Constructs a new GetPlayerRoomsReq.
         * @memberof biz
         * @classdesc Represents a GetPlayerRoomsReq.
         * @implements IGetPlayerRoomsReq
         * @constructor
         * @param {biz.IGetPlayerRoomsReq=} [properties] Properties to set
         */
        function GetPlayerRoomsReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetPlayerRoomsReq playerId.
         * @member {number|Long} playerId
         * @memberof biz.GetPlayerRoomsReq
         * @instance
         */
        GetPlayerRoomsReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new GetPlayerRoomsReq instance using the specified properties.
         * @function create
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {biz.IGetPlayerRoomsReq=} [properties] Properties to set
         * @returns {biz.GetPlayerRoomsReq} GetPlayerRoomsReq instance
         */
        GetPlayerRoomsReq.create = function create(properties) {
            return new GetPlayerRoomsReq(properties);
        };

        /**
         * Encodes the specified GetPlayerRoomsReq message. Does not implicitly {@link biz.GetPlayerRoomsReq.verify|verify} messages.
         * @function encode
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {biz.IGetPlayerRoomsReq} message GetPlayerRoomsReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRoomsReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified GetPlayerRoomsReq message, length delimited. Does not implicitly {@link biz.GetPlayerRoomsReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {biz.IGetPlayerRoomsReq} message GetPlayerRoomsReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRoomsReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetPlayerRoomsReq message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetPlayerRoomsReq} GetPlayerRoomsReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRoomsReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetPlayerRoomsReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetPlayerRoomsReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetPlayerRoomsReq} GetPlayerRoomsReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRoomsReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetPlayerRoomsReq message.
         * @function verify
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetPlayerRoomsReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a GetPlayerRoomsReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetPlayerRoomsReq} GetPlayerRoomsReq
         */
        GetPlayerRoomsReq.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetPlayerRoomsReq)
                return object;
            var message = new $root.biz.GetPlayerRoomsReq();
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a GetPlayerRoomsReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {biz.GetPlayerRoomsReq} message GetPlayerRoomsReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetPlayerRoomsReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this GetPlayerRoomsReq to JSON.
         * @function toJSON
         * @memberof biz.GetPlayerRoomsReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetPlayerRoomsReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetPlayerRoomsReq
         * @function getTypeUrl
         * @memberof biz.GetPlayerRoomsReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetPlayerRoomsReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetPlayerRoomsReq";
        };

        return GetPlayerRoomsReq;
    })();

    biz.GetPlayerRoomsRes = (function() {

        /**
         * Properties of a GetPlayerRoomsRes.
         * @memberof biz
         * @interface IGetPlayerRoomsRes
         * @property {common.IResult|null} [result] GetPlayerRoomsRes result
         * @property {Array.<chat.IChatRoomInfo>|null} [rooms] GetPlayerRoomsRes rooms
         */

        /**
         * Constructs a new GetPlayerRoomsRes.
         * @memberof biz
         * @classdesc Represents a GetPlayerRoomsRes.
         * @implements IGetPlayerRoomsRes
         * @constructor
         * @param {biz.IGetPlayerRoomsRes=} [properties] Properties to set
         */
        function GetPlayerRoomsRes(properties) {
            this.rooms = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * GetPlayerRoomsRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof biz.GetPlayerRoomsRes
         * @instance
         */
        GetPlayerRoomsRes.prototype.result = null;

        /**
         * GetPlayerRoomsRes rooms.
         * @member {Array.<chat.IChatRoomInfo>} rooms
         * @memberof biz.GetPlayerRoomsRes
         * @instance
         */
        GetPlayerRoomsRes.prototype.rooms = $util.emptyArray;

        /**
         * Creates a new GetPlayerRoomsRes instance using the specified properties.
         * @function create
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {biz.IGetPlayerRoomsRes=} [properties] Properties to set
         * @returns {biz.GetPlayerRoomsRes} GetPlayerRoomsRes instance
         */
        GetPlayerRoomsRes.create = function create(properties) {
            return new GetPlayerRoomsRes(properties);
        };

        /**
         * Encodes the specified GetPlayerRoomsRes message. Does not implicitly {@link biz.GetPlayerRoomsRes.verify|verify} messages.
         * @function encode
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {biz.IGetPlayerRoomsRes} message GetPlayerRoomsRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRoomsRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.rooms != null && message.rooms.length)
                for (var i = 0; i < message.rooms.length; ++i)
                    $root.chat.ChatRoomInfo.encode(message.rooms[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified GetPlayerRoomsRes message, length delimited. Does not implicitly {@link biz.GetPlayerRoomsRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {biz.IGetPlayerRoomsRes} message GetPlayerRoomsRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        GetPlayerRoomsRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a GetPlayerRoomsRes message from the specified reader or buffer.
         * @function decode
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.GetPlayerRoomsRes} GetPlayerRoomsRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRoomsRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.GetPlayerRoomsRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        if (!(message.rooms && message.rooms.length))
                            message.rooms = [];
                        message.rooms.push($root.chat.ChatRoomInfo.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a GetPlayerRoomsRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.GetPlayerRoomsRes} GetPlayerRoomsRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        GetPlayerRoomsRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a GetPlayerRoomsRes message.
         * @function verify
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        GetPlayerRoomsRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.rooms != null && message.hasOwnProperty("rooms")) {
                if (!Array.isArray(message.rooms))
                    return "rooms: array expected";
                for (var i = 0; i < message.rooms.length; ++i) {
                    var error = $root.chat.ChatRoomInfo.verify(message.rooms[i]);
                    if (error)
                        return "rooms." + error;
                }
            }
            return null;
        };

        /**
         * Creates a GetPlayerRoomsRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.GetPlayerRoomsRes} GetPlayerRoomsRes
         */
        GetPlayerRoomsRes.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.GetPlayerRoomsRes)
                return object;
            var message = new $root.biz.GetPlayerRoomsRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".biz.GetPlayerRoomsRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.rooms) {
                if (!Array.isArray(object.rooms))
                    throw TypeError(".biz.GetPlayerRoomsRes.rooms: array expected");
                message.rooms = [];
                for (var i = 0; i < object.rooms.length; ++i) {
                    if (typeof object.rooms[i] !== "object")
                        throw TypeError(".biz.GetPlayerRoomsRes.rooms: object expected");
                    message.rooms[i] = $root.chat.ChatRoomInfo.fromObject(object.rooms[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a GetPlayerRoomsRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {biz.GetPlayerRoomsRes} message GetPlayerRoomsRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        GetPlayerRoomsRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.rooms = [];
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.rooms && message.rooms.length) {
                object.rooms = [];
                for (var j = 0; j < message.rooms.length; ++j)
                    object.rooms[j] = $root.chat.ChatRoomInfo.toObject(message.rooms[j], options);
            }
            return object;
        };

        /**
         * Converts this GetPlayerRoomsRes to JSON.
         * @function toJSON
         * @memberof biz.GetPlayerRoomsRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        GetPlayerRoomsRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for GetPlayerRoomsRes
         * @function getTypeUrl
         * @memberof biz.GetPlayerRoomsRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        GetPlayerRoomsRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.GetPlayerRoomsRes";
        };

        return GetPlayerRoomsRes;
    })();

    biz.Ping = (function() {

        /**
         * Properties of a Ping.
         * @memberof biz
         * @interface IPing
         * @property {number|Long|null} [clientTime] Ping clientTime
         */

        /**
         * Constructs a new Ping.
         * @memberof biz
         * @classdesc Represents a Ping.
         * @implements IPing
         * @constructor
         * @param {biz.IPing=} [properties] Properties to set
         */
        function Ping(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Ping clientTime.
         * @member {number|Long} clientTime
         * @memberof biz.Ping
         * @instance
         */
        Ping.prototype.clientTime = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new Ping instance using the specified properties.
         * @function create
         * @memberof biz.Ping
         * @static
         * @param {biz.IPing=} [properties] Properties to set
         * @returns {biz.Ping} Ping instance
         */
        Ping.create = function create(properties) {
            return new Ping(properties);
        };

        /**
         * Encodes the specified Ping message. Does not implicitly {@link biz.Ping.verify|verify} messages.
         * @function encode
         * @memberof biz.Ping
         * @static
         * @param {biz.IPing} message Ping message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Ping.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.clientTime != null && Object.hasOwnProperty.call(message, "clientTime"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.clientTime);
            return writer;
        };

        /**
         * Encodes the specified Ping message, length delimited. Does not implicitly {@link biz.Ping.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.Ping
         * @static
         * @param {biz.IPing} message Ping message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Ping.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a Ping message from the specified reader or buffer.
         * @function decode
         * @memberof biz.Ping
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.Ping} Ping
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Ping.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.Ping();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.clientTime = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a Ping message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.Ping
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.Ping} Ping
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Ping.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a Ping message.
         * @function verify
         * @memberof biz.Ping
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Ping.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.clientTime != null && message.hasOwnProperty("clientTime"))
                if (!$util.isInteger(message.clientTime) && !(message.clientTime && $util.isInteger(message.clientTime.low) && $util.isInteger(message.clientTime.high)))
                    return "clientTime: integer|Long expected";
            return null;
        };

        /**
         * Creates a Ping message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.Ping
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.Ping} Ping
         */
        Ping.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.Ping)
                return object;
            var message = new $root.biz.Ping();
            if (object.clientTime != null)
                if ($util.Long)
                    (message.clientTime = $util.Long.fromValue(object.clientTime)).unsigned = true;
                else if (typeof object.clientTime === "string")
                    message.clientTime = parseInt(object.clientTime, 10);
                else if (typeof object.clientTime === "number")
                    message.clientTime = object.clientTime;
                else if (typeof object.clientTime === "object")
                    message.clientTime = new $util.LongBits(object.clientTime.low >>> 0, object.clientTime.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a Ping message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.Ping
         * @static
         * @param {biz.Ping} message Ping
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Ping.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.clientTime = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.clientTime = options.longs === String ? "0" : 0;
            if (message.clientTime != null && message.hasOwnProperty("clientTime"))
                if (typeof message.clientTime === "number")
                    object.clientTime = options.longs === String ? String(message.clientTime) : message.clientTime;
                else
                    object.clientTime = options.longs === String ? $util.Long.prototype.toString.call(message.clientTime) : options.longs === Number ? new $util.LongBits(message.clientTime.low >>> 0, message.clientTime.high >>> 0).toNumber(true) : message.clientTime;
            return object;
        };

        /**
         * Converts this Ping to JSON.
         * @function toJSON
         * @memberof biz.Ping
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Ping.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Ping
         * @function getTypeUrl
         * @memberof biz.Ping
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Ping.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.Ping";
        };

        return Ping;
    })();

    biz.Pong = (function() {

        /**
         * Properties of a Pong.
         * @memberof biz
         * @interface IPong
         * @property {number|Long|null} [clientTime] Pong clientTime
         * @property {number|Long|null} [serverTime] Pong serverTime
         */

        /**
         * Constructs a new Pong.
         * @memberof biz
         * @classdesc Represents a Pong.
         * @implements IPong
         * @constructor
         * @param {biz.IPong=} [properties] Properties to set
         */
        function Pong(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Pong clientTime.
         * @member {number|Long} clientTime
         * @memberof biz.Pong
         * @instance
         */
        Pong.prototype.clientTime = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Pong serverTime.
         * @member {number|Long} serverTime
         * @memberof biz.Pong
         * @instance
         */
        Pong.prototype.serverTime = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new Pong instance using the specified properties.
         * @function create
         * @memberof biz.Pong
         * @static
         * @param {biz.IPong=} [properties] Properties to set
         * @returns {biz.Pong} Pong instance
         */
        Pong.create = function create(properties) {
            return new Pong(properties);
        };

        /**
         * Encodes the specified Pong message. Does not implicitly {@link biz.Pong.verify|verify} messages.
         * @function encode
         * @memberof biz.Pong
         * @static
         * @param {biz.IPong} message Pong message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Pong.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.clientTime != null && Object.hasOwnProperty.call(message, "clientTime"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.clientTime);
            if (message.serverTime != null && Object.hasOwnProperty.call(message, "serverTime"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.serverTime);
            return writer;
        };

        /**
         * Encodes the specified Pong message, length delimited. Does not implicitly {@link biz.Pong.verify|verify} messages.
         * @function encodeDelimited
         * @memberof biz.Pong
         * @static
         * @param {biz.IPong} message Pong message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Pong.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a Pong message from the specified reader or buffer.
         * @function decode
         * @memberof biz.Pong
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {biz.Pong} Pong
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Pong.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.biz.Pong();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.clientTime = reader.uint64();
                        break;
                    }
                case 2: {
                        message.serverTime = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a Pong message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof biz.Pong
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {biz.Pong} Pong
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Pong.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a Pong message.
         * @function verify
         * @memberof biz.Pong
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Pong.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.clientTime != null && message.hasOwnProperty("clientTime"))
                if (!$util.isInteger(message.clientTime) && !(message.clientTime && $util.isInteger(message.clientTime.low) && $util.isInteger(message.clientTime.high)))
                    return "clientTime: integer|Long expected";
            if (message.serverTime != null && message.hasOwnProperty("serverTime"))
                if (!$util.isInteger(message.serverTime) && !(message.serverTime && $util.isInteger(message.serverTime.low) && $util.isInteger(message.serverTime.high)))
                    return "serverTime: integer|Long expected";
            return null;
        };

        /**
         * Creates a Pong message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof biz.Pong
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {biz.Pong} Pong
         */
        Pong.fromObject = function fromObject(object) {
            if (object instanceof $root.biz.Pong)
                return object;
            var message = new $root.biz.Pong();
            if (object.clientTime != null)
                if ($util.Long)
                    (message.clientTime = $util.Long.fromValue(object.clientTime)).unsigned = true;
                else if (typeof object.clientTime === "string")
                    message.clientTime = parseInt(object.clientTime, 10);
                else if (typeof object.clientTime === "number")
                    message.clientTime = object.clientTime;
                else if (typeof object.clientTime === "object")
                    message.clientTime = new $util.LongBits(object.clientTime.low >>> 0, object.clientTime.high >>> 0).toNumber(true);
            if (object.serverTime != null)
                if ($util.Long)
                    (message.serverTime = $util.Long.fromValue(object.serverTime)).unsigned = true;
                else if (typeof object.serverTime === "string")
                    message.serverTime = parseInt(object.serverTime, 10);
                else if (typeof object.serverTime === "number")
                    message.serverTime = object.serverTime;
                else if (typeof object.serverTime === "object")
                    message.serverTime = new $util.LongBits(object.serverTime.low >>> 0, object.serverTime.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a Pong message. Also converts values to other types if specified.
         * @function toObject
         * @memberof biz.Pong
         * @static
         * @param {biz.Pong} message Pong
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Pong.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.clientTime = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.clientTime = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.serverTime = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.serverTime = options.longs === String ? "0" : 0;
            }
            if (message.clientTime != null && message.hasOwnProperty("clientTime"))
                if (typeof message.clientTime === "number")
                    object.clientTime = options.longs === String ? String(message.clientTime) : message.clientTime;
                else
                    object.clientTime = options.longs === String ? $util.Long.prototype.toString.call(message.clientTime) : options.longs === Number ? new $util.LongBits(message.clientTime.low >>> 0, message.clientTime.high >>> 0).toNumber(true) : message.clientTime;
            if (message.serverTime != null && message.hasOwnProperty("serverTime"))
                if (typeof message.serverTime === "number")
                    object.serverTime = options.longs === String ? String(message.serverTime) : message.serverTime;
                else
                    object.serverTime = options.longs === String ? $util.Long.prototype.toString.call(message.serverTime) : options.longs === Number ? new $util.LongBits(message.serverTime.low >>> 0, message.serverTime.high >>> 0).toNumber(true) : message.serverTime;
            return object;
        };

        /**
         * Converts this Pong to JSON.
         * @function toJSON
         * @memberof biz.Pong
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Pong.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Pong
         * @function getTypeUrl
         * @memberof biz.Pong
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Pong.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/biz.Pong";
        };

        return Pong;
    })();

    return biz;
})();

$root.common = (function() {

    /**
     * Namespace common.
     * @exports common
     * @namespace
     */
    var common = {};

    common.Result = (function() {

        /**
         * Properties of a Result.
         * @memberof common
         * @interface IResult
         * @property {boolean|null} [ok] Result ok
         * @property {number|null} [code] Result code
         * @property {string|null} [msg] Result msg
         */

        /**
         * Constructs a new Result.
         * @memberof common
         * @classdesc Represents a Result.
         * @implements IResult
         * @constructor
         * @param {common.IResult=} [properties] Properties to set
         */
        function Result(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Result ok.
         * @member {boolean} ok
         * @memberof common.Result
         * @instance
         */
        Result.prototype.ok = false;

        /**
         * Result code.
         * @member {number} code
         * @memberof common.Result
         * @instance
         */
        Result.prototype.code = 0;

        /**
         * Result msg.
         * @member {string} msg
         * @memberof common.Result
         * @instance
         */
        Result.prototype.msg = "";

        /**
         * Creates a new Result instance using the specified properties.
         * @function create
         * @memberof common.Result
         * @static
         * @param {common.IResult=} [properties] Properties to set
         * @returns {common.Result} Result instance
         */
        Result.create = function create(properties) {
            return new Result(properties);
        };

        /**
         * Encodes the specified Result message. Does not implicitly {@link common.Result.verify|verify} messages.
         * @function encode
         * @memberof common.Result
         * @static
         * @param {common.IResult} message Result message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Result.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.ok != null && Object.hasOwnProperty.call(message, "ok"))
                writer.uint32(/* id 1, wireType 0 =*/8).bool(message.ok);
            if (message.code != null && Object.hasOwnProperty.call(message, "code"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.code);
            if (message.msg != null && Object.hasOwnProperty.call(message, "msg"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.msg);
            return writer;
        };

        /**
         * Encodes the specified Result message, length delimited. Does not implicitly {@link common.Result.verify|verify} messages.
         * @function encodeDelimited
         * @memberof common.Result
         * @static
         * @param {common.IResult} message Result message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Result.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a Result message from the specified reader or buffer.
         * @function decode
         * @memberof common.Result
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {common.Result} Result
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Result.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.common.Result();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.ok = reader.bool();
                        break;
                    }
                case 2: {
                        message.code = reader.uint32();
                        break;
                    }
                case 3: {
                        message.msg = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a Result message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof common.Result
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {common.Result} Result
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Result.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a Result message.
         * @function verify
         * @memberof common.Result
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Result.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.ok != null && message.hasOwnProperty("ok"))
                if (typeof message.ok !== "boolean")
                    return "ok: boolean expected";
            if (message.code != null && message.hasOwnProperty("code"))
                if (!$util.isInteger(message.code))
                    return "code: integer expected";
            if (message.msg != null && message.hasOwnProperty("msg"))
                if (!$util.isString(message.msg))
                    return "msg: string expected";
            return null;
        };

        /**
         * Creates a Result message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof common.Result
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {common.Result} Result
         */
        Result.fromObject = function fromObject(object) {
            if (object instanceof $root.common.Result)
                return object;
            var message = new $root.common.Result();
            if (object.ok != null)
                message.ok = Boolean(object.ok);
            if (object.code != null)
                message.code = object.code >>> 0;
            if (object.msg != null)
                message.msg = String(object.msg);
            return message;
        };

        /**
         * Creates a plain object from a Result message. Also converts values to other types if specified.
         * @function toObject
         * @memberof common.Result
         * @static
         * @param {common.Result} message Result
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Result.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.ok = false;
                object.code = 0;
                object.msg = "";
            }
            if (message.ok != null && message.hasOwnProperty("ok"))
                object.ok = message.ok;
            if (message.code != null && message.hasOwnProperty("code"))
                object.code = message.code;
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = message.msg;
            return object;
        };

        /**
         * Converts this Result to JSON.
         * @function toJSON
         * @memberof common.Result
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Result.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Result
         * @function getTypeUrl
         * @memberof common.Result
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Result.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/common.Result";
        };

        return Result;
    })();

    common.PageReq = (function() {

        /**
         * Properties of a PageReq.
         * @memberof common
         * @interface IPageReq
         * @property {number|null} [page] PageReq page
         * @property {number|null} [limit] PageReq limit
         */

        /**
         * Constructs a new PageReq.
         * @memberof common
         * @classdesc Represents a PageReq.
         * @implements IPageReq
         * @constructor
         * @param {common.IPageReq=} [properties] Properties to set
         */
        function PageReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * PageReq page.
         * @member {number} page
         * @memberof common.PageReq
         * @instance
         */
        PageReq.prototype.page = 0;

        /**
         * PageReq limit.
         * @member {number} limit
         * @memberof common.PageReq
         * @instance
         */
        PageReq.prototype.limit = 0;

        /**
         * Creates a new PageReq instance using the specified properties.
         * @function create
         * @memberof common.PageReq
         * @static
         * @param {common.IPageReq=} [properties] Properties to set
         * @returns {common.PageReq} PageReq instance
         */
        PageReq.create = function create(properties) {
            return new PageReq(properties);
        };

        /**
         * Encodes the specified PageReq message. Does not implicitly {@link common.PageReq.verify|verify} messages.
         * @function encode
         * @memberof common.PageReq
         * @static
         * @param {common.IPageReq} message PageReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PageReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.page != null && Object.hasOwnProperty.call(message, "page"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.page);
            if (message.limit != null && Object.hasOwnProperty.call(message, "limit"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.limit);
            return writer;
        };

        /**
         * Encodes the specified PageReq message, length delimited. Does not implicitly {@link common.PageReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof common.PageReq
         * @static
         * @param {common.IPageReq} message PageReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PageReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a PageReq message from the specified reader or buffer.
         * @function decode
         * @memberof common.PageReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {common.PageReq} PageReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PageReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.common.PageReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.page = reader.uint32();
                        break;
                    }
                case 2: {
                        message.limit = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a PageReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof common.PageReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {common.PageReq} PageReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PageReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a PageReq message.
         * @function verify
         * @memberof common.PageReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        PageReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.page != null && message.hasOwnProperty("page"))
                if (!$util.isInteger(message.page))
                    return "page: integer expected";
            if (message.limit != null && message.hasOwnProperty("limit"))
                if (!$util.isInteger(message.limit))
                    return "limit: integer expected";
            return null;
        };

        /**
         * Creates a PageReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof common.PageReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {common.PageReq} PageReq
         */
        PageReq.fromObject = function fromObject(object) {
            if (object instanceof $root.common.PageReq)
                return object;
            var message = new $root.common.PageReq();
            if (object.page != null)
                message.page = object.page >>> 0;
            if (object.limit != null)
                message.limit = object.limit >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a PageReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof common.PageReq
         * @static
         * @param {common.PageReq} message PageReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        PageReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.page = 0;
                object.limit = 0;
            }
            if (message.page != null && message.hasOwnProperty("page"))
                object.page = message.page;
            if (message.limit != null && message.hasOwnProperty("limit"))
                object.limit = message.limit;
            return object;
        };

        /**
         * Converts this PageReq to JSON.
         * @function toJSON
         * @memberof common.PageReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        PageReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for PageReq
         * @function getTypeUrl
         * @memberof common.PageReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        PageReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/common.PageReq";
        };

        return PageReq;
    })();

    common.PageRes = (function() {

        /**
         * Properties of a PageRes.
         * @memberof common
         * @interface IPageRes
         * @property {number|null} [page] PageRes page
         * @property {number|null} [limit] PageRes limit
         * @property {number|null} [total] PageRes total
         * @property {number|null} [totalPage] PageRes totalPage
         */

        /**
         * Constructs a new PageRes.
         * @memberof common
         * @classdesc Represents a PageRes.
         * @implements IPageRes
         * @constructor
         * @param {common.IPageRes=} [properties] Properties to set
         */
        function PageRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * PageRes page.
         * @member {number} page
         * @memberof common.PageRes
         * @instance
         */
        PageRes.prototype.page = 0;

        /**
         * PageRes limit.
         * @member {number} limit
         * @memberof common.PageRes
         * @instance
         */
        PageRes.prototype.limit = 0;

        /**
         * PageRes total.
         * @member {number} total
         * @memberof common.PageRes
         * @instance
         */
        PageRes.prototype.total = 0;

        /**
         * PageRes totalPage.
         * @member {number} totalPage
         * @memberof common.PageRes
         * @instance
         */
        PageRes.prototype.totalPage = 0;

        /**
         * Creates a new PageRes instance using the specified properties.
         * @function create
         * @memberof common.PageRes
         * @static
         * @param {common.IPageRes=} [properties] Properties to set
         * @returns {common.PageRes} PageRes instance
         */
        PageRes.create = function create(properties) {
            return new PageRes(properties);
        };

        /**
         * Encodes the specified PageRes message. Does not implicitly {@link common.PageRes.verify|verify} messages.
         * @function encode
         * @memberof common.PageRes
         * @static
         * @param {common.IPageRes} message PageRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PageRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.page != null && Object.hasOwnProperty.call(message, "page"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.page);
            if (message.limit != null && Object.hasOwnProperty.call(message, "limit"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.limit);
            if (message.total != null && Object.hasOwnProperty.call(message, "total"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.total);
            if (message.totalPage != null && Object.hasOwnProperty.call(message, "totalPage"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint32(message.totalPage);
            return writer;
        };

        /**
         * Encodes the specified PageRes message, length delimited. Does not implicitly {@link common.PageRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof common.PageRes
         * @static
         * @param {common.IPageRes} message PageRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        PageRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a PageRes message from the specified reader or buffer.
         * @function decode
         * @memberof common.PageRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {common.PageRes} PageRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PageRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.common.PageRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.page = reader.uint32();
                        break;
                    }
                case 2: {
                        message.limit = reader.uint32();
                        break;
                    }
                case 3: {
                        message.total = reader.uint32();
                        break;
                    }
                case 4: {
                        message.totalPage = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a PageRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof common.PageRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {common.PageRes} PageRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        PageRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a PageRes message.
         * @function verify
         * @memberof common.PageRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        PageRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.page != null && message.hasOwnProperty("page"))
                if (!$util.isInteger(message.page))
                    return "page: integer expected";
            if (message.limit != null && message.hasOwnProperty("limit"))
                if (!$util.isInteger(message.limit))
                    return "limit: integer expected";
            if (message.total != null && message.hasOwnProperty("total"))
                if (!$util.isInteger(message.total))
                    return "total: integer expected";
            if (message.totalPage != null && message.hasOwnProperty("totalPage"))
                if (!$util.isInteger(message.totalPage))
                    return "totalPage: integer expected";
            return null;
        };

        /**
         * Creates a PageRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof common.PageRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {common.PageRes} PageRes
         */
        PageRes.fromObject = function fromObject(object) {
            if (object instanceof $root.common.PageRes)
                return object;
            var message = new $root.common.PageRes();
            if (object.page != null)
                message.page = object.page >>> 0;
            if (object.limit != null)
                message.limit = object.limit >>> 0;
            if (object.total != null)
                message.total = object.total >>> 0;
            if (object.totalPage != null)
                message.totalPage = object.totalPage >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a PageRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof common.PageRes
         * @static
         * @param {common.PageRes} message PageRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        PageRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.page = 0;
                object.limit = 0;
                object.total = 0;
                object.totalPage = 0;
            }
            if (message.page != null && message.hasOwnProperty("page"))
                object.page = message.page;
            if (message.limit != null && message.hasOwnProperty("limit"))
                object.limit = message.limit;
            if (message.total != null && message.hasOwnProperty("total"))
                object.total = message.total;
            if (message.totalPage != null && message.hasOwnProperty("totalPage"))
                object.totalPage = message.totalPage;
            return object;
        };

        /**
         * Converts this PageRes to JSON.
         * @function toJSON
         * @memberof common.PageRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        PageRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for PageRes
         * @function getTypeUrl
         * @memberof common.PageRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        PageRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/common.PageRes";
        };

        return PageRes;
    })();

    common.Empty = (function() {

        /**
         * Properties of an Empty.
         * @memberof common
         * @interface IEmpty
         */

        /**
         * Constructs a new Empty.
         * @memberof common
         * @classdesc Represents an Empty.
         * @implements IEmpty
         * @constructor
         * @param {common.IEmpty=} [properties] Properties to set
         */
        function Empty(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Creates a new Empty instance using the specified properties.
         * @function create
         * @memberof common.Empty
         * @static
         * @param {common.IEmpty=} [properties] Properties to set
         * @returns {common.Empty} Empty instance
         */
        Empty.create = function create(properties) {
            return new Empty(properties);
        };

        /**
         * Encodes the specified Empty message. Does not implicitly {@link common.Empty.verify|verify} messages.
         * @function encode
         * @memberof common.Empty
         * @static
         * @param {common.IEmpty} message Empty message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Empty.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            return writer;
        };

        /**
         * Encodes the specified Empty message, length delimited. Does not implicitly {@link common.Empty.verify|verify} messages.
         * @function encodeDelimited
         * @memberof common.Empty
         * @static
         * @param {common.IEmpty} message Empty message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Empty.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes an Empty message from the specified reader or buffer.
         * @function decode
         * @memberof common.Empty
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {common.Empty} Empty
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Empty.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.common.Empty();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes an Empty message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof common.Empty
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {common.Empty} Empty
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Empty.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies an Empty message.
         * @function verify
         * @memberof common.Empty
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Empty.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            return null;
        };

        /**
         * Creates an Empty message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof common.Empty
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {common.Empty} Empty
         */
        Empty.fromObject = function fromObject(object) {
            if (object instanceof $root.common.Empty)
                return object;
            return new $root.common.Empty();
        };

        /**
         * Creates a plain object from an Empty message. Also converts values to other types if specified.
         * @function toObject
         * @memberof common.Empty
         * @static
         * @param {common.Empty} message Empty
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Empty.toObject = function toObject() {
            return {};
        };

        /**
         * Converts this Empty to JSON.
         * @function toJSON
         * @memberof common.Empty
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Empty.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Empty
         * @function getTypeUrl
         * @memberof common.Empty
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Empty.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/common.Empty";
        };

        return Empty;
    })();

    common.Packet = (function() {

        /**
         * Properties of a Packet.
         * @memberof common
         * @interface IPacket
         * @property {number|null} [magic] Packet magic
         * @property {number|null} [cmdId] Packet cmdId
         * @property {number|null} [seqId] Packet seqId
         * @property {number|null} [flag] Packet flag
         * @property {Uint8Array|null} [payload] Packet payload
         */

        /**
         * Constructs a new Packet.
         * @memberof common
         * @classdesc Represents a Packet.
         * @implements IPacket
         * @constructor
         * @param {common.IPacket=} [properties] Properties to set
         */
        function Packet(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Packet magic.
         * @member {number} magic
         * @memberof common.Packet
         * @instance
         */
        Packet.prototype.magic = 0;

        /**
         * Packet cmdId.
         * @member {number} cmdId
         * @memberof common.Packet
         * @instance
         */
        Packet.prototype.cmdId = 0;

        /**
         * Packet seqId.
         * @member {number} seqId
         * @memberof common.Packet
         * @instance
         */
        Packet.prototype.seqId = 0;

        /**
         * Packet flag.
         * @member {number} flag
         * @memberof common.Packet
         * @instance
         */
        Packet.prototype.flag = 0;

        /**
         * Packet payload.
         * @member {Uint8Array} payload
         * @memberof common.Packet
         * @instance
         */
        Packet.prototype.payload = $util.newBuffer([]);

        /**
         * Creates a new Packet instance using the specified properties.
         * @function create
         * @memberof common.Packet
         * @static
         * @param {common.IPacket=} [properties] Properties to set
         * @returns {common.Packet} Packet instance
         */
        Packet.create = function create(properties) {
            return new Packet(properties);
        };

        /**
         * Encodes the specified Packet message. Does not implicitly {@link common.Packet.verify|verify} messages.
         * @function encode
         * @memberof common.Packet
         * @static
         * @param {common.IPacket} message Packet message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Packet.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.magic != null && Object.hasOwnProperty.call(message, "magic"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.magic);
            if (message.cmdId != null && Object.hasOwnProperty.call(message, "cmdId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.cmdId);
            if (message.seqId != null && Object.hasOwnProperty.call(message, "seqId"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.seqId);
            if (message.flag != null && Object.hasOwnProperty.call(message, "flag"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint32(message.flag);
            if (message.payload != null && Object.hasOwnProperty.call(message, "payload"))
                writer.uint32(/* id 5, wireType 2 =*/42).bytes(message.payload);
            return writer;
        };

        /**
         * Encodes the specified Packet message, length delimited. Does not implicitly {@link common.Packet.verify|verify} messages.
         * @function encodeDelimited
         * @memberof common.Packet
         * @static
         * @param {common.IPacket} message Packet message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Packet.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a Packet message from the specified reader or buffer.
         * @function decode
         * @memberof common.Packet
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {common.Packet} Packet
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Packet.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.common.Packet();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.magic = reader.uint32();
                        break;
                    }
                case 2: {
                        message.cmdId = reader.uint32();
                        break;
                    }
                case 3: {
                        message.seqId = reader.uint32();
                        break;
                    }
                case 4: {
                        message.flag = reader.uint32();
                        break;
                    }
                case 5: {
                        message.payload = reader.bytes();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a Packet message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof common.Packet
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {common.Packet} Packet
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Packet.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a Packet message.
         * @function verify
         * @memberof common.Packet
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Packet.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.magic != null && message.hasOwnProperty("magic"))
                if (!$util.isInteger(message.magic))
                    return "magic: integer expected";
            if (message.cmdId != null && message.hasOwnProperty("cmdId"))
                if (!$util.isInteger(message.cmdId))
                    return "cmdId: integer expected";
            if (message.seqId != null && message.hasOwnProperty("seqId"))
                if (!$util.isInteger(message.seqId))
                    return "seqId: integer expected";
            if (message.flag != null && message.hasOwnProperty("flag"))
                if (!$util.isInteger(message.flag))
                    return "flag: integer expected";
            if (message.payload != null && message.hasOwnProperty("payload"))
                if (!(message.payload && typeof message.payload.length === "number" || $util.isString(message.payload)))
                    return "payload: buffer expected";
            return null;
        };

        /**
         * Creates a Packet message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof common.Packet
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {common.Packet} Packet
         */
        Packet.fromObject = function fromObject(object) {
            if (object instanceof $root.common.Packet)
                return object;
            var message = new $root.common.Packet();
            if (object.magic != null)
                message.magic = object.magic >>> 0;
            if (object.cmdId != null)
                message.cmdId = object.cmdId >>> 0;
            if (object.seqId != null)
                message.seqId = object.seqId >>> 0;
            if (object.flag != null)
                message.flag = object.flag >>> 0;
            if (object.payload != null)
                if (typeof object.payload === "string")
                    $util.base64.decode(object.payload, message.payload = $util.newBuffer($util.base64.length(object.payload)), 0);
                else if (object.payload.length >= 0)
                    message.payload = object.payload;
            return message;
        };

        /**
         * Creates a plain object from a Packet message. Also converts values to other types if specified.
         * @function toObject
         * @memberof common.Packet
         * @static
         * @param {common.Packet} message Packet
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Packet.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.magic = 0;
                object.cmdId = 0;
                object.seqId = 0;
                object.flag = 0;
                if (options.bytes === String)
                    object.payload = "";
                else {
                    object.payload = [];
                    if (options.bytes !== Array)
                        object.payload = $util.newBuffer(object.payload);
                }
            }
            if (message.magic != null && message.hasOwnProperty("magic"))
                object.magic = message.magic;
            if (message.cmdId != null && message.hasOwnProperty("cmdId"))
                object.cmdId = message.cmdId;
            if (message.seqId != null && message.hasOwnProperty("seqId"))
                object.seqId = message.seqId;
            if (message.flag != null && message.hasOwnProperty("flag"))
                object.flag = message.flag;
            if (message.payload != null && message.hasOwnProperty("payload"))
                object.payload = options.bytes === String ? $util.base64.encode(message.payload, 0, message.payload.length) : options.bytes === Array ? Array.prototype.slice.call(message.payload) : message.payload;
            return object;
        };

        /**
         * Converts this Packet to JSON.
         * @function toJSON
         * @memberof common.Packet
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Packet.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Packet
         * @function getTypeUrl
         * @memberof common.Packet
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Packet.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/common.Packet";
        };

        return Packet;
    })();

    return common;
})();

$root.chat = (function() {

    /**
     * Namespace chat.
     * @exports chat
     * @namespace
     */
    var chat = {};

    chat.ChatRoomInfo = (function() {

        /**
         * Properties of a ChatRoomInfo.
         * @memberof chat
         * @interface IChatRoomInfo
         * @property {number|Long|null} [roomId] ChatRoomInfo roomId
         * @property {string|null} [name] ChatRoomInfo name
         * @property {number|Long|null} [creatorId] ChatRoomInfo creatorId
         * @property {number|null} [status] ChatRoomInfo status
         * @property {number|Long|null} [createdAt] ChatRoomInfo createdAt
         * @property {number|Long|null} [closedAt] ChatRoomInfo closedAt
         */

        /**
         * Constructs a new ChatRoomInfo.
         * @memberof chat
         * @classdesc Represents a ChatRoomInfo.
         * @implements IChatRoomInfo
         * @constructor
         * @param {chat.IChatRoomInfo=} [properties] Properties to set
         */
        function ChatRoomInfo(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatRoomInfo roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatRoomInfo name.
         * @member {string} name
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.name = "";

        /**
         * ChatRoomInfo creatorId.
         * @member {number|Long} creatorId
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.creatorId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatRoomInfo status.
         * @member {number} status
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.status = 0;

        /**
         * ChatRoomInfo createdAt.
         * @member {number|Long} createdAt
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.createdAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatRoomInfo closedAt.
         * @member {number|Long} closedAt
         * @memberof chat.ChatRoomInfo
         * @instance
         */
        ChatRoomInfo.prototype.closedAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatRoomInfo instance using the specified properties.
         * @function create
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {chat.IChatRoomInfo=} [properties] Properties to set
         * @returns {chat.ChatRoomInfo} ChatRoomInfo instance
         */
        ChatRoomInfo.create = function create(properties) {
            return new ChatRoomInfo(properties);
        };

        /**
         * Encodes the specified ChatRoomInfo message. Does not implicitly {@link chat.ChatRoomInfo.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {chat.IChatRoomInfo} message ChatRoomInfo message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatRoomInfo.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.name);
            if (message.creatorId != null && Object.hasOwnProperty.call(message, "creatorId"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint64(message.creatorId);
            if (message.status != null && Object.hasOwnProperty.call(message, "status"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint32(message.status);
            if (message.createdAt != null && Object.hasOwnProperty.call(message, "createdAt"))
                writer.uint32(/* id 5, wireType 0 =*/40).uint64(message.createdAt);
            if (message.closedAt != null && Object.hasOwnProperty.call(message, "closedAt"))
                writer.uint32(/* id 6, wireType 0 =*/48).uint64(message.closedAt);
            return writer;
        };

        /**
         * Encodes the specified ChatRoomInfo message, length delimited. Does not implicitly {@link chat.ChatRoomInfo.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {chat.IChatRoomInfo} message ChatRoomInfo message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatRoomInfo.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatRoomInfo message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatRoomInfo} ChatRoomInfo
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatRoomInfo.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatRoomInfo();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.name = reader.string();
                        break;
                    }
                case 3: {
                        message.creatorId = reader.uint64();
                        break;
                    }
                case 4: {
                        message.status = reader.uint32();
                        break;
                    }
                case 5: {
                        message.createdAt = reader.uint64();
                        break;
                    }
                case 6: {
                        message.closedAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatRoomInfo message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatRoomInfo} ChatRoomInfo
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatRoomInfo.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatRoomInfo message.
         * @function verify
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatRoomInfo.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.name != null && message.hasOwnProperty("name"))
                if (!$util.isString(message.name))
                    return "name: string expected";
            if (message.creatorId != null && message.hasOwnProperty("creatorId"))
                if (!$util.isInteger(message.creatorId) && !(message.creatorId && $util.isInteger(message.creatorId.low) && $util.isInteger(message.creatorId.high)))
                    return "creatorId: integer|Long expected";
            if (message.status != null && message.hasOwnProperty("status"))
                if (!$util.isInteger(message.status))
                    return "status: integer expected";
            if (message.createdAt != null && message.hasOwnProperty("createdAt"))
                if (!$util.isInteger(message.createdAt) && !(message.createdAt && $util.isInteger(message.createdAt.low) && $util.isInteger(message.createdAt.high)))
                    return "createdAt: integer|Long expected";
            if (message.closedAt != null && message.hasOwnProperty("closedAt"))
                if (!$util.isInteger(message.closedAt) && !(message.closedAt && $util.isInteger(message.closedAt.low) && $util.isInteger(message.closedAt.high)))
                    return "closedAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatRoomInfo message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatRoomInfo} ChatRoomInfo
         */
        ChatRoomInfo.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatRoomInfo)
                return object;
            var message = new $root.chat.ChatRoomInfo();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.name != null)
                message.name = String(object.name);
            if (object.creatorId != null)
                if ($util.Long)
                    (message.creatorId = $util.Long.fromValue(object.creatorId)).unsigned = true;
                else if (typeof object.creatorId === "string")
                    message.creatorId = parseInt(object.creatorId, 10);
                else if (typeof object.creatorId === "number")
                    message.creatorId = object.creatorId;
                else if (typeof object.creatorId === "object")
                    message.creatorId = new $util.LongBits(object.creatorId.low >>> 0, object.creatorId.high >>> 0).toNumber(true);
            if (object.status != null)
                message.status = object.status >>> 0;
            if (object.createdAt != null)
                if ($util.Long)
                    (message.createdAt = $util.Long.fromValue(object.createdAt)).unsigned = true;
                else if (typeof object.createdAt === "string")
                    message.createdAt = parseInt(object.createdAt, 10);
                else if (typeof object.createdAt === "number")
                    message.createdAt = object.createdAt;
                else if (typeof object.createdAt === "object")
                    message.createdAt = new $util.LongBits(object.createdAt.low >>> 0, object.createdAt.high >>> 0).toNumber(true);
            if (object.closedAt != null)
                if ($util.Long)
                    (message.closedAt = $util.Long.fromValue(object.closedAt)).unsigned = true;
                else if (typeof object.closedAt === "string")
                    message.closedAt = parseInt(object.closedAt, 10);
                else if (typeof object.closedAt === "number")
                    message.closedAt = object.closedAt;
                else if (typeof object.closedAt === "object")
                    message.closedAt = new $util.LongBits(object.closedAt.low >>> 0, object.closedAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatRoomInfo message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {chat.ChatRoomInfo} message ChatRoomInfo
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatRoomInfo.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                object.name = "";
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.creatorId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.creatorId = options.longs === String ? "0" : 0;
                object.status = 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.createdAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.createdAt = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.closedAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.closedAt = options.longs === String ? "0" : 0;
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.creatorId != null && message.hasOwnProperty("creatorId"))
                if (typeof message.creatorId === "number")
                    object.creatorId = options.longs === String ? String(message.creatorId) : message.creatorId;
                else
                    object.creatorId = options.longs === String ? $util.Long.prototype.toString.call(message.creatorId) : options.longs === Number ? new $util.LongBits(message.creatorId.low >>> 0, message.creatorId.high >>> 0).toNumber(true) : message.creatorId;
            if (message.status != null && message.hasOwnProperty("status"))
                object.status = message.status;
            if (message.createdAt != null && message.hasOwnProperty("createdAt"))
                if (typeof message.createdAt === "number")
                    object.createdAt = options.longs === String ? String(message.createdAt) : message.createdAt;
                else
                    object.createdAt = options.longs === String ? $util.Long.prototype.toString.call(message.createdAt) : options.longs === Number ? new $util.LongBits(message.createdAt.low >>> 0, message.createdAt.high >>> 0).toNumber(true) : message.createdAt;
            if (message.closedAt != null && message.hasOwnProperty("closedAt"))
                if (typeof message.closedAt === "number")
                    object.closedAt = options.longs === String ? String(message.closedAt) : message.closedAt;
                else
                    object.closedAt = options.longs === String ? $util.Long.prototype.toString.call(message.closedAt) : options.longs === Number ? new $util.LongBits(message.closedAt.low >>> 0, message.closedAt.high >>> 0).toNumber(true) : message.closedAt;
            return object;
        };

        /**
         * Converts this ChatRoomInfo to JSON.
         * @function toJSON
         * @memberof chat.ChatRoomInfo
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatRoomInfo.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatRoomInfo
         * @function getTypeUrl
         * @memberof chat.ChatRoomInfo
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatRoomInfo.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatRoomInfo";
        };

        return ChatRoomInfo;
    })();

    chat.ChatMessage = (function() {

        /**
         * Properties of a ChatMessage.
         * @memberof chat
         * @interface IChatMessage
         * @property {number|Long|null} [msgId] ChatMessage msgId
         * @property {number|Long|null} [roomId] ChatMessage roomId
         * @property {number|Long|null} [senderId] ChatMessage senderId
         * @property {string|null} [senderName] ChatMessage senderName
         * @property {string|null} [content] ChatMessage content
         * @property {number|Long|null} [sentAt] ChatMessage sentAt
         */

        /**
         * Constructs a new ChatMessage.
         * @memberof chat
         * @classdesc Represents a ChatMessage.
         * @implements IChatMessage
         * @constructor
         * @param {chat.IChatMessage=} [properties] Properties to set
         */
        function ChatMessage(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatMessage msgId.
         * @member {number|Long} msgId
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.msgId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatMessage roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatMessage senderId.
         * @member {number|Long} senderId
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.senderId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatMessage senderName.
         * @member {string} senderName
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.senderName = "";

        /**
         * ChatMessage content.
         * @member {string} content
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.content = "";

        /**
         * ChatMessage sentAt.
         * @member {number|Long} sentAt
         * @memberof chat.ChatMessage
         * @instance
         */
        ChatMessage.prototype.sentAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatMessage instance using the specified properties.
         * @function create
         * @memberof chat.ChatMessage
         * @static
         * @param {chat.IChatMessage=} [properties] Properties to set
         * @returns {chat.ChatMessage} ChatMessage instance
         */
        ChatMessage.create = function create(properties) {
            return new ChatMessage(properties);
        };

        /**
         * Encodes the specified ChatMessage message. Does not implicitly {@link chat.ChatMessage.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatMessage
         * @static
         * @param {chat.IChatMessage} message ChatMessage message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatMessage.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.msgId != null && Object.hasOwnProperty.call(message, "msgId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.msgId);
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.roomId);
            if (message.senderId != null && Object.hasOwnProperty.call(message, "senderId"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint64(message.senderId);
            if (message.senderName != null && Object.hasOwnProperty.call(message, "senderName"))
                writer.uint32(/* id 4, wireType 2 =*/34).string(message.senderName);
            if (message.content != null && Object.hasOwnProperty.call(message, "content"))
                writer.uint32(/* id 5, wireType 2 =*/42).string(message.content);
            if (message.sentAt != null && Object.hasOwnProperty.call(message, "sentAt"))
                writer.uint32(/* id 6, wireType 0 =*/48).uint64(message.sentAt);
            return writer;
        };

        /**
         * Encodes the specified ChatMessage message, length delimited. Does not implicitly {@link chat.ChatMessage.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatMessage
         * @static
         * @param {chat.IChatMessage} message ChatMessage message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatMessage.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatMessage message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatMessage
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatMessage} ChatMessage
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatMessage.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatMessage();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.msgId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 3: {
                        message.senderId = reader.uint64();
                        break;
                    }
                case 4: {
                        message.senderName = reader.string();
                        break;
                    }
                case 5: {
                        message.content = reader.string();
                        break;
                    }
                case 6: {
                        message.sentAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatMessage message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatMessage
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatMessage} ChatMessage
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatMessage.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatMessage message.
         * @function verify
         * @memberof chat.ChatMessage
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatMessage.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.msgId != null && message.hasOwnProperty("msgId"))
                if (!$util.isInteger(message.msgId) && !(message.msgId && $util.isInteger(message.msgId.low) && $util.isInteger(message.msgId.high)))
                    return "msgId: integer|Long expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.senderId != null && message.hasOwnProperty("senderId"))
                if (!$util.isInteger(message.senderId) && !(message.senderId && $util.isInteger(message.senderId.low) && $util.isInteger(message.senderId.high)))
                    return "senderId: integer|Long expected";
            if (message.senderName != null && message.hasOwnProperty("senderName"))
                if (!$util.isString(message.senderName))
                    return "senderName: string expected";
            if (message.content != null && message.hasOwnProperty("content"))
                if (!$util.isString(message.content))
                    return "content: string expected";
            if (message.sentAt != null && message.hasOwnProperty("sentAt"))
                if (!$util.isInteger(message.sentAt) && !(message.sentAt && $util.isInteger(message.sentAt.low) && $util.isInteger(message.sentAt.high)))
                    return "sentAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatMessage message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatMessage
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatMessage} ChatMessage
         */
        ChatMessage.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatMessage)
                return object;
            var message = new $root.chat.ChatMessage();
            if (object.msgId != null)
                if ($util.Long)
                    (message.msgId = $util.Long.fromValue(object.msgId)).unsigned = true;
                else if (typeof object.msgId === "string")
                    message.msgId = parseInt(object.msgId, 10);
                else if (typeof object.msgId === "number")
                    message.msgId = object.msgId;
                else if (typeof object.msgId === "object")
                    message.msgId = new $util.LongBits(object.msgId.low >>> 0, object.msgId.high >>> 0).toNumber(true);
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.senderId != null)
                if ($util.Long)
                    (message.senderId = $util.Long.fromValue(object.senderId)).unsigned = true;
                else if (typeof object.senderId === "string")
                    message.senderId = parseInt(object.senderId, 10);
                else if (typeof object.senderId === "number")
                    message.senderId = object.senderId;
                else if (typeof object.senderId === "object")
                    message.senderId = new $util.LongBits(object.senderId.low >>> 0, object.senderId.high >>> 0).toNumber(true);
            if (object.senderName != null)
                message.senderName = String(object.senderName);
            if (object.content != null)
                message.content = String(object.content);
            if (object.sentAt != null)
                if ($util.Long)
                    (message.sentAt = $util.Long.fromValue(object.sentAt)).unsigned = true;
                else if (typeof object.sentAt === "string")
                    message.sentAt = parseInt(object.sentAt, 10);
                else if (typeof object.sentAt === "number")
                    message.sentAt = object.sentAt;
                else if (typeof object.sentAt === "object")
                    message.sentAt = new $util.LongBits(object.sentAt.low >>> 0, object.sentAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatMessage message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatMessage
         * @static
         * @param {chat.ChatMessage} message ChatMessage
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatMessage.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.msgId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.msgId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.senderId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.senderId = options.longs === String ? "0" : 0;
                object.senderName = "";
                object.content = "";
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.sentAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.sentAt = options.longs === String ? "0" : 0;
            }
            if (message.msgId != null && message.hasOwnProperty("msgId"))
                if (typeof message.msgId === "number")
                    object.msgId = options.longs === String ? String(message.msgId) : message.msgId;
                else
                    object.msgId = options.longs === String ? $util.Long.prototype.toString.call(message.msgId) : options.longs === Number ? new $util.LongBits(message.msgId.low >>> 0, message.msgId.high >>> 0).toNumber(true) : message.msgId;
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.senderId != null && message.hasOwnProperty("senderId"))
                if (typeof message.senderId === "number")
                    object.senderId = options.longs === String ? String(message.senderId) : message.senderId;
                else
                    object.senderId = options.longs === String ? $util.Long.prototype.toString.call(message.senderId) : options.longs === Number ? new $util.LongBits(message.senderId.low >>> 0, message.senderId.high >>> 0).toNumber(true) : message.senderId;
            if (message.senderName != null && message.hasOwnProperty("senderName"))
                object.senderName = message.senderName;
            if (message.content != null && message.hasOwnProperty("content"))
                object.content = message.content;
            if (message.sentAt != null && message.hasOwnProperty("sentAt"))
                if (typeof message.sentAt === "number")
                    object.sentAt = options.longs === String ? String(message.sentAt) : message.sentAt;
                else
                    object.sentAt = options.longs === String ? $util.Long.prototype.toString.call(message.sentAt) : options.longs === Number ? new $util.LongBits(message.sentAt.low >>> 0, message.sentAt.high >>> 0).toNumber(true) : message.sentAt;
            return object;
        };

        /**
         * Converts this ChatMessage to JSON.
         * @function toJSON
         * @memberof chat.ChatMessage
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatMessage.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatMessage
         * @function getTypeUrl
         * @memberof chat.ChatMessage
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatMessage.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatMessage";
        };

        return ChatMessage;
    })();

    chat.ChatCreateRoomReq = (function() {

        /**
         * Properties of a ChatCreateRoomReq.
         * @memberof chat
         * @interface IChatCreateRoomReq
         * @property {string|null} [name] ChatCreateRoomReq name
         * @property {number|Long|null} [creatorId] ChatCreateRoomReq creatorId
         */

        /**
         * Constructs a new ChatCreateRoomReq.
         * @memberof chat
         * @classdesc Represents a ChatCreateRoomReq.
         * @implements IChatCreateRoomReq
         * @constructor
         * @param {chat.IChatCreateRoomReq=} [properties] Properties to set
         */
        function ChatCreateRoomReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatCreateRoomReq name.
         * @member {string} name
         * @memberof chat.ChatCreateRoomReq
         * @instance
         */
        ChatCreateRoomReq.prototype.name = "";

        /**
         * ChatCreateRoomReq creatorId.
         * @member {number|Long} creatorId
         * @memberof chat.ChatCreateRoomReq
         * @instance
         */
        ChatCreateRoomReq.prototype.creatorId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatCreateRoomReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {chat.IChatCreateRoomReq=} [properties] Properties to set
         * @returns {chat.ChatCreateRoomReq} ChatCreateRoomReq instance
         */
        ChatCreateRoomReq.create = function create(properties) {
            return new ChatCreateRoomReq(properties);
        };

        /**
         * Encodes the specified ChatCreateRoomReq message. Does not implicitly {@link chat.ChatCreateRoomReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {chat.IChatCreateRoomReq} message ChatCreateRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCreateRoomReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
            if (message.creatorId != null && Object.hasOwnProperty.call(message, "creatorId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.creatorId);
            return writer;
        };

        /**
         * Encodes the specified ChatCreateRoomReq message, length delimited. Does not implicitly {@link chat.ChatCreateRoomReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {chat.IChatCreateRoomReq} message ChatCreateRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCreateRoomReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatCreateRoomReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatCreateRoomReq} ChatCreateRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCreateRoomReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatCreateRoomReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.name = reader.string();
                        break;
                    }
                case 2: {
                        message.creatorId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatCreateRoomReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatCreateRoomReq} ChatCreateRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCreateRoomReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatCreateRoomReq message.
         * @function verify
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatCreateRoomReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.name != null && message.hasOwnProperty("name"))
                if (!$util.isString(message.name))
                    return "name: string expected";
            if (message.creatorId != null && message.hasOwnProperty("creatorId"))
                if (!$util.isInteger(message.creatorId) && !(message.creatorId && $util.isInteger(message.creatorId.low) && $util.isInteger(message.creatorId.high)))
                    return "creatorId: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatCreateRoomReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatCreateRoomReq} ChatCreateRoomReq
         */
        ChatCreateRoomReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatCreateRoomReq)
                return object;
            var message = new $root.chat.ChatCreateRoomReq();
            if (object.name != null)
                message.name = String(object.name);
            if (object.creatorId != null)
                if ($util.Long)
                    (message.creatorId = $util.Long.fromValue(object.creatorId)).unsigned = true;
                else if (typeof object.creatorId === "string")
                    message.creatorId = parseInt(object.creatorId, 10);
                else if (typeof object.creatorId === "number")
                    message.creatorId = object.creatorId;
                else if (typeof object.creatorId === "object")
                    message.creatorId = new $util.LongBits(object.creatorId.low >>> 0, object.creatorId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatCreateRoomReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {chat.ChatCreateRoomReq} message ChatCreateRoomReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatCreateRoomReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.name = "";
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.creatorId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.creatorId = options.longs === String ? "0" : 0;
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.creatorId != null && message.hasOwnProperty("creatorId"))
                if (typeof message.creatorId === "number")
                    object.creatorId = options.longs === String ? String(message.creatorId) : message.creatorId;
                else
                    object.creatorId = options.longs === String ? $util.Long.prototype.toString.call(message.creatorId) : options.longs === Number ? new $util.LongBits(message.creatorId.low >>> 0, message.creatorId.high >>> 0).toNumber(true) : message.creatorId;
            return object;
        };

        /**
         * Converts this ChatCreateRoomReq to JSON.
         * @function toJSON
         * @memberof chat.ChatCreateRoomReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatCreateRoomReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatCreateRoomReq
         * @function getTypeUrl
         * @memberof chat.ChatCreateRoomReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatCreateRoomReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatCreateRoomReq";
        };

        return ChatCreateRoomReq;
    })();

    chat.ChatCreateRoomRes = (function() {

        /**
         * Properties of a ChatCreateRoomRes.
         * @memberof chat
         * @interface IChatCreateRoomRes
         * @property {common.IResult|null} [result] ChatCreateRoomRes result
         * @property {chat.IChatRoomInfo|null} [room] ChatCreateRoomRes room
         */

        /**
         * Constructs a new ChatCreateRoomRes.
         * @memberof chat
         * @classdesc Represents a ChatCreateRoomRes.
         * @implements IChatCreateRoomRes
         * @constructor
         * @param {chat.IChatCreateRoomRes=} [properties] Properties to set
         */
        function ChatCreateRoomRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatCreateRoomRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatCreateRoomRes
         * @instance
         */
        ChatCreateRoomRes.prototype.result = null;

        /**
         * ChatCreateRoomRes room.
         * @member {chat.IChatRoomInfo|null|undefined} room
         * @memberof chat.ChatCreateRoomRes
         * @instance
         */
        ChatCreateRoomRes.prototype.room = null;

        /**
         * Creates a new ChatCreateRoomRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {chat.IChatCreateRoomRes=} [properties] Properties to set
         * @returns {chat.ChatCreateRoomRes} ChatCreateRoomRes instance
         */
        ChatCreateRoomRes.create = function create(properties) {
            return new ChatCreateRoomRes(properties);
        };

        /**
         * Encodes the specified ChatCreateRoomRes message. Does not implicitly {@link chat.ChatCreateRoomRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {chat.IChatCreateRoomRes} message ChatCreateRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCreateRoomRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.room != null && Object.hasOwnProperty.call(message, "room"))
                $root.chat.ChatRoomInfo.encode(message.room, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatCreateRoomRes message, length delimited. Does not implicitly {@link chat.ChatCreateRoomRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {chat.IChatCreateRoomRes} message ChatCreateRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCreateRoomRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatCreateRoomRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatCreateRoomRes} ChatCreateRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCreateRoomRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatCreateRoomRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.room = $root.chat.ChatRoomInfo.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatCreateRoomRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatCreateRoomRes} ChatCreateRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCreateRoomRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatCreateRoomRes message.
         * @function verify
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatCreateRoomRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.room != null && message.hasOwnProperty("room")) {
                var error = $root.chat.ChatRoomInfo.verify(message.room);
                if (error)
                    return "room." + error;
            }
            return null;
        };

        /**
         * Creates a ChatCreateRoomRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatCreateRoomRes} ChatCreateRoomRes
         */
        ChatCreateRoomRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatCreateRoomRes)
                return object;
            var message = new $root.chat.ChatCreateRoomRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatCreateRoomRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.room != null) {
                if (typeof object.room !== "object")
                    throw TypeError(".chat.ChatCreateRoomRes.room: object expected");
                message.room = $root.chat.ChatRoomInfo.fromObject(object.room);
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatCreateRoomRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {chat.ChatCreateRoomRes} message ChatCreateRoomRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatCreateRoomRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                object.room = null;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.room != null && message.hasOwnProperty("room"))
                object.room = $root.chat.ChatRoomInfo.toObject(message.room, options);
            return object;
        };

        /**
         * Converts this ChatCreateRoomRes to JSON.
         * @function toJSON
         * @memberof chat.ChatCreateRoomRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatCreateRoomRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatCreateRoomRes
         * @function getTypeUrl
         * @memberof chat.ChatCreateRoomRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatCreateRoomRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatCreateRoomRes";
        };

        return ChatCreateRoomRes;
    })();

    chat.ChatJoinRoomReq = (function() {

        /**
         * Properties of a ChatJoinRoomReq.
         * @memberof chat
         * @interface IChatJoinRoomReq
         * @property {number|Long|null} [roomId] ChatJoinRoomReq roomId
         * @property {number|Long|null} [playerId] ChatJoinRoomReq playerId
         */

        /**
         * Constructs a new ChatJoinRoomReq.
         * @memberof chat
         * @classdesc Represents a ChatJoinRoomReq.
         * @implements IChatJoinRoomReq
         * @constructor
         * @param {chat.IChatJoinRoomReq=} [properties] Properties to set
         */
        function ChatJoinRoomReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatJoinRoomReq roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatJoinRoomReq
         * @instance
         */
        ChatJoinRoomReq.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatJoinRoomReq playerId.
         * @member {number|Long} playerId
         * @memberof chat.ChatJoinRoomReq
         * @instance
         */
        ChatJoinRoomReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatJoinRoomReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {chat.IChatJoinRoomReq=} [properties] Properties to set
         * @returns {chat.ChatJoinRoomReq} ChatJoinRoomReq instance
         */
        ChatJoinRoomReq.create = function create(properties) {
            return new ChatJoinRoomReq(properties);
        };

        /**
         * Encodes the specified ChatJoinRoomReq message. Does not implicitly {@link chat.ChatJoinRoomReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {chat.IChatJoinRoomReq} message ChatJoinRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatJoinRoomReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified ChatJoinRoomReq message, length delimited. Does not implicitly {@link chat.ChatJoinRoomReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {chat.IChatJoinRoomReq} message ChatJoinRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatJoinRoomReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatJoinRoomReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatJoinRoomReq} ChatJoinRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatJoinRoomReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatJoinRoomReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatJoinRoomReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatJoinRoomReq} ChatJoinRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatJoinRoomReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatJoinRoomReq message.
         * @function verify
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatJoinRoomReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatJoinRoomReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatJoinRoomReq} ChatJoinRoomReq
         */
        ChatJoinRoomReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatJoinRoomReq)
                return object;
            var message = new $root.chat.ChatJoinRoomReq();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatJoinRoomReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {chat.ChatJoinRoomReq} message ChatJoinRoomReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatJoinRoomReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this ChatJoinRoomReq to JSON.
         * @function toJSON
         * @memberof chat.ChatJoinRoomReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatJoinRoomReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatJoinRoomReq
         * @function getTypeUrl
         * @memberof chat.ChatJoinRoomReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatJoinRoomReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatJoinRoomReq";
        };

        return ChatJoinRoomReq;
    })();

    chat.ChatJoinRoomRes = (function() {

        /**
         * Properties of a ChatJoinRoomRes.
         * @memberof chat
         * @interface IChatJoinRoomRes
         * @property {common.IResult|null} [result] ChatJoinRoomRes result
         * @property {chat.IChatRoomInfo|null} [room] ChatJoinRoomRes room
         * @property {Array.<chat.IChatMessage>|null} [recentMsgs] ChatJoinRoomRes recentMsgs
         */

        /**
         * Constructs a new ChatJoinRoomRes.
         * @memberof chat
         * @classdesc Represents a ChatJoinRoomRes.
         * @implements IChatJoinRoomRes
         * @constructor
         * @param {chat.IChatJoinRoomRes=} [properties] Properties to set
         */
        function ChatJoinRoomRes(properties) {
            this.recentMsgs = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatJoinRoomRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatJoinRoomRes
         * @instance
         */
        ChatJoinRoomRes.prototype.result = null;

        /**
         * ChatJoinRoomRes room.
         * @member {chat.IChatRoomInfo|null|undefined} room
         * @memberof chat.ChatJoinRoomRes
         * @instance
         */
        ChatJoinRoomRes.prototype.room = null;

        /**
         * ChatJoinRoomRes recentMsgs.
         * @member {Array.<chat.IChatMessage>} recentMsgs
         * @memberof chat.ChatJoinRoomRes
         * @instance
         */
        ChatJoinRoomRes.prototype.recentMsgs = $util.emptyArray;

        /**
         * Creates a new ChatJoinRoomRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {chat.IChatJoinRoomRes=} [properties] Properties to set
         * @returns {chat.ChatJoinRoomRes} ChatJoinRoomRes instance
         */
        ChatJoinRoomRes.create = function create(properties) {
            return new ChatJoinRoomRes(properties);
        };

        /**
         * Encodes the specified ChatJoinRoomRes message. Does not implicitly {@link chat.ChatJoinRoomRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {chat.IChatJoinRoomRes} message ChatJoinRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatJoinRoomRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.room != null && Object.hasOwnProperty.call(message, "room"))
                $root.chat.ChatRoomInfo.encode(message.room, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            if (message.recentMsgs != null && message.recentMsgs.length)
                for (var i = 0; i < message.recentMsgs.length; ++i)
                    $root.chat.ChatMessage.encode(message.recentMsgs[i], writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatJoinRoomRes message, length delimited. Does not implicitly {@link chat.ChatJoinRoomRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {chat.IChatJoinRoomRes} message ChatJoinRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatJoinRoomRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatJoinRoomRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatJoinRoomRes} ChatJoinRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatJoinRoomRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatJoinRoomRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.room = $root.chat.ChatRoomInfo.decode(reader, reader.uint32());
                        break;
                    }
                case 3: {
                        if (!(message.recentMsgs && message.recentMsgs.length))
                            message.recentMsgs = [];
                        message.recentMsgs.push($root.chat.ChatMessage.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatJoinRoomRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatJoinRoomRes} ChatJoinRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatJoinRoomRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatJoinRoomRes message.
         * @function verify
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatJoinRoomRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.room != null && message.hasOwnProperty("room")) {
                var error = $root.chat.ChatRoomInfo.verify(message.room);
                if (error)
                    return "room." + error;
            }
            if (message.recentMsgs != null && message.hasOwnProperty("recentMsgs")) {
                if (!Array.isArray(message.recentMsgs))
                    return "recentMsgs: array expected";
                for (var i = 0; i < message.recentMsgs.length; ++i) {
                    var error = $root.chat.ChatMessage.verify(message.recentMsgs[i]);
                    if (error)
                        return "recentMsgs." + error;
                }
            }
            return null;
        };

        /**
         * Creates a ChatJoinRoomRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatJoinRoomRes} ChatJoinRoomRes
         */
        ChatJoinRoomRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatJoinRoomRes)
                return object;
            var message = new $root.chat.ChatJoinRoomRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatJoinRoomRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.room != null) {
                if (typeof object.room !== "object")
                    throw TypeError(".chat.ChatJoinRoomRes.room: object expected");
                message.room = $root.chat.ChatRoomInfo.fromObject(object.room);
            }
            if (object.recentMsgs) {
                if (!Array.isArray(object.recentMsgs))
                    throw TypeError(".chat.ChatJoinRoomRes.recentMsgs: array expected");
                message.recentMsgs = [];
                for (var i = 0; i < object.recentMsgs.length; ++i) {
                    if (typeof object.recentMsgs[i] !== "object")
                        throw TypeError(".chat.ChatJoinRoomRes.recentMsgs: object expected");
                    message.recentMsgs[i] = $root.chat.ChatMessage.fromObject(object.recentMsgs[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatJoinRoomRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {chat.ChatJoinRoomRes} message ChatJoinRoomRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatJoinRoomRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.recentMsgs = [];
            if (options.defaults) {
                object.result = null;
                object.room = null;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.room != null && message.hasOwnProperty("room"))
                object.room = $root.chat.ChatRoomInfo.toObject(message.room, options);
            if (message.recentMsgs && message.recentMsgs.length) {
                object.recentMsgs = [];
                for (var j = 0; j < message.recentMsgs.length; ++j)
                    object.recentMsgs[j] = $root.chat.ChatMessage.toObject(message.recentMsgs[j], options);
            }
            return object;
        };

        /**
         * Converts this ChatJoinRoomRes to JSON.
         * @function toJSON
         * @memberof chat.ChatJoinRoomRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatJoinRoomRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatJoinRoomRes
         * @function getTypeUrl
         * @memberof chat.ChatJoinRoomRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatJoinRoomRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatJoinRoomRes";
        };

        return ChatJoinRoomRes;
    })();

    chat.ChatLeaveRoomReq = (function() {

        /**
         * Properties of a ChatLeaveRoomReq.
         * @memberof chat
         * @interface IChatLeaveRoomReq
         * @property {number|Long|null} [roomId] ChatLeaveRoomReq roomId
         * @property {number|Long|null} [playerId] ChatLeaveRoomReq playerId
         */

        /**
         * Constructs a new ChatLeaveRoomReq.
         * @memberof chat
         * @classdesc Represents a ChatLeaveRoomReq.
         * @implements IChatLeaveRoomReq
         * @constructor
         * @param {chat.IChatLeaveRoomReq=} [properties] Properties to set
         */
        function ChatLeaveRoomReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatLeaveRoomReq roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatLeaveRoomReq
         * @instance
         */
        ChatLeaveRoomReq.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatLeaveRoomReq playerId.
         * @member {number|Long} playerId
         * @memberof chat.ChatLeaveRoomReq
         * @instance
         */
        ChatLeaveRoomReq.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatLeaveRoomReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {chat.IChatLeaveRoomReq=} [properties] Properties to set
         * @returns {chat.ChatLeaveRoomReq} ChatLeaveRoomReq instance
         */
        ChatLeaveRoomReq.create = function create(properties) {
            return new ChatLeaveRoomReq(properties);
        };

        /**
         * Encodes the specified ChatLeaveRoomReq message. Does not implicitly {@link chat.ChatLeaveRoomReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {chat.IChatLeaveRoomReq} message ChatLeaveRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatLeaveRoomReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified ChatLeaveRoomReq message, length delimited. Does not implicitly {@link chat.ChatLeaveRoomReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {chat.IChatLeaveRoomReq} message ChatLeaveRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatLeaveRoomReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatLeaveRoomReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatLeaveRoomReq} ChatLeaveRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatLeaveRoomReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatLeaveRoomReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatLeaveRoomReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatLeaveRoomReq} ChatLeaveRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatLeaveRoomReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatLeaveRoomReq message.
         * @function verify
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatLeaveRoomReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatLeaveRoomReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatLeaveRoomReq} ChatLeaveRoomReq
         */
        ChatLeaveRoomReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatLeaveRoomReq)
                return object;
            var message = new $root.chat.ChatLeaveRoomReq();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatLeaveRoomReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {chat.ChatLeaveRoomReq} message ChatLeaveRoomReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatLeaveRoomReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this ChatLeaveRoomReq to JSON.
         * @function toJSON
         * @memberof chat.ChatLeaveRoomReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatLeaveRoomReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatLeaveRoomReq
         * @function getTypeUrl
         * @memberof chat.ChatLeaveRoomReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatLeaveRoomReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatLeaveRoomReq";
        };

        return ChatLeaveRoomReq;
    })();

    chat.ChatLeaveRoomRes = (function() {

        /**
         * Properties of a ChatLeaveRoomRes.
         * @memberof chat
         * @interface IChatLeaveRoomRes
         * @property {common.IResult|null} [result] ChatLeaveRoomRes result
         */

        /**
         * Constructs a new ChatLeaveRoomRes.
         * @memberof chat
         * @classdesc Represents a ChatLeaveRoomRes.
         * @implements IChatLeaveRoomRes
         * @constructor
         * @param {chat.IChatLeaveRoomRes=} [properties] Properties to set
         */
        function ChatLeaveRoomRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatLeaveRoomRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatLeaveRoomRes
         * @instance
         */
        ChatLeaveRoomRes.prototype.result = null;

        /**
         * Creates a new ChatLeaveRoomRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {chat.IChatLeaveRoomRes=} [properties] Properties to set
         * @returns {chat.ChatLeaveRoomRes} ChatLeaveRoomRes instance
         */
        ChatLeaveRoomRes.create = function create(properties) {
            return new ChatLeaveRoomRes(properties);
        };

        /**
         * Encodes the specified ChatLeaveRoomRes message. Does not implicitly {@link chat.ChatLeaveRoomRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {chat.IChatLeaveRoomRes} message ChatLeaveRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatLeaveRoomRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatLeaveRoomRes message, length delimited. Does not implicitly {@link chat.ChatLeaveRoomRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {chat.IChatLeaveRoomRes} message ChatLeaveRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatLeaveRoomRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatLeaveRoomRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatLeaveRoomRes} ChatLeaveRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatLeaveRoomRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatLeaveRoomRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatLeaveRoomRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatLeaveRoomRes} ChatLeaveRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatLeaveRoomRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatLeaveRoomRes message.
         * @function verify
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatLeaveRoomRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            return null;
        };

        /**
         * Creates a ChatLeaveRoomRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatLeaveRoomRes} ChatLeaveRoomRes
         */
        ChatLeaveRoomRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatLeaveRoomRes)
                return object;
            var message = new $root.chat.ChatLeaveRoomRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatLeaveRoomRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatLeaveRoomRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {chat.ChatLeaveRoomRes} message ChatLeaveRoomRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatLeaveRoomRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            return object;
        };

        /**
         * Converts this ChatLeaveRoomRes to JSON.
         * @function toJSON
         * @memberof chat.ChatLeaveRoomRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatLeaveRoomRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatLeaveRoomRes
         * @function getTypeUrl
         * @memberof chat.ChatLeaveRoomRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatLeaveRoomRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatLeaveRoomRes";
        };

        return ChatLeaveRoomRes;
    })();

    chat.ChatSendMsgReq = (function() {

        /**
         * Properties of a ChatSendMsgReq.
         * @memberof chat
         * @interface IChatSendMsgReq
         * @property {number|Long|null} [roomId] ChatSendMsgReq roomId
         * @property {number|Long|null} [senderId] ChatSendMsgReq senderId
         * @property {string|null} [content] ChatSendMsgReq content
         */

        /**
         * Constructs a new ChatSendMsgReq.
         * @memberof chat
         * @classdesc Represents a ChatSendMsgReq.
         * @implements IChatSendMsgReq
         * @constructor
         * @param {chat.IChatSendMsgReq=} [properties] Properties to set
         */
        function ChatSendMsgReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatSendMsgReq roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatSendMsgReq
         * @instance
         */
        ChatSendMsgReq.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatSendMsgReq senderId.
         * @member {number|Long} senderId
         * @memberof chat.ChatSendMsgReq
         * @instance
         */
        ChatSendMsgReq.prototype.senderId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatSendMsgReq content.
         * @member {string} content
         * @memberof chat.ChatSendMsgReq
         * @instance
         */
        ChatSendMsgReq.prototype.content = "";

        /**
         * Creates a new ChatSendMsgReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {chat.IChatSendMsgReq=} [properties] Properties to set
         * @returns {chat.ChatSendMsgReq} ChatSendMsgReq instance
         */
        ChatSendMsgReq.create = function create(properties) {
            return new ChatSendMsgReq(properties);
        };

        /**
         * Encodes the specified ChatSendMsgReq message. Does not implicitly {@link chat.ChatSendMsgReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {chat.IChatSendMsgReq} message ChatSendMsgReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatSendMsgReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.senderId != null && Object.hasOwnProperty.call(message, "senderId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.senderId);
            if (message.content != null && Object.hasOwnProperty.call(message, "content"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.content);
            return writer;
        };

        /**
         * Encodes the specified ChatSendMsgReq message, length delimited. Does not implicitly {@link chat.ChatSendMsgReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {chat.IChatSendMsgReq} message ChatSendMsgReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatSendMsgReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatSendMsgReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatSendMsgReq} ChatSendMsgReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatSendMsgReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatSendMsgReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.senderId = reader.uint64();
                        break;
                    }
                case 3: {
                        message.content = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatSendMsgReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatSendMsgReq} ChatSendMsgReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatSendMsgReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatSendMsgReq message.
         * @function verify
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatSendMsgReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.senderId != null && message.hasOwnProperty("senderId"))
                if (!$util.isInteger(message.senderId) && !(message.senderId && $util.isInteger(message.senderId.low) && $util.isInteger(message.senderId.high)))
                    return "senderId: integer|Long expected";
            if (message.content != null && message.hasOwnProperty("content"))
                if (!$util.isString(message.content))
                    return "content: string expected";
            return null;
        };

        /**
         * Creates a ChatSendMsgReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatSendMsgReq} ChatSendMsgReq
         */
        ChatSendMsgReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatSendMsgReq)
                return object;
            var message = new $root.chat.ChatSendMsgReq();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.senderId != null)
                if ($util.Long)
                    (message.senderId = $util.Long.fromValue(object.senderId)).unsigned = true;
                else if (typeof object.senderId === "string")
                    message.senderId = parseInt(object.senderId, 10);
                else if (typeof object.senderId === "number")
                    message.senderId = object.senderId;
                else if (typeof object.senderId === "object")
                    message.senderId = new $util.LongBits(object.senderId.low >>> 0, object.senderId.high >>> 0).toNumber(true);
            if (object.content != null)
                message.content = String(object.content);
            return message;
        };

        /**
         * Creates a plain object from a ChatSendMsgReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {chat.ChatSendMsgReq} message ChatSendMsgReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatSendMsgReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.senderId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.senderId = options.longs === String ? "0" : 0;
                object.content = "";
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.senderId != null && message.hasOwnProperty("senderId"))
                if (typeof message.senderId === "number")
                    object.senderId = options.longs === String ? String(message.senderId) : message.senderId;
                else
                    object.senderId = options.longs === String ? $util.Long.prototype.toString.call(message.senderId) : options.longs === Number ? new $util.LongBits(message.senderId.low >>> 0, message.senderId.high >>> 0).toNumber(true) : message.senderId;
            if (message.content != null && message.hasOwnProperty("content"))
                object.content = message.content;
            return object;
        };

        /**
         * Converts this ChatSendMsgReq to JSON.
         * @function toJSON
         * @memberof chat.ChatSendMsgReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatSendMsgReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatSendMsgReq
         * @function getTypeUrl
         * @memberof chat.ChatSendMsgReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatSendMsgReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatSendMsgReq";
        };

        return ChatSendMsgReq;
    })();

    chat.ChatSendMsgRes = (function() {

        /**
         * Properties of a ChatSendMsgRes.
         * @memberof chat
         * @interface IChatSendMsgRes
         * @property {common.IResult|null} [result] ChatSendMsgRes result
         * @property {chat.IChatMessage|null} [msg] ChatSendMsgRes msg
         */

        /**
         * Constructs a new ChatSendMsgRes.
         * @memberof chat
         * @classdesc Represents a ChatSendMsgRes.
         * @implements IChatSendMsgRes
         * @constructor
         * @param {chat.IChatSendMsgRes=} [properties] Properties to set
         */
        function ChatSendMsgRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatSendMsgRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatSendMsgRes
         * @instance
         */
        ChatSendMsgRes.prototype.result = null;

        /**
         * ChatSendMsgRes msg.
         * @member {chat.IChatMessage|null|undefined} msg
         * @memberof chat.ChatSendMsgRes
         * @instance
         */
        ChatSendMsgRes.prototype.msg = null;

        /**
         * Creates a new ChatSendMsgRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {chat.IChatSendMsgRes=} [properties] Properties to set
         * @returns {chat.ChatSendMsgRes} ChatSendMsgRes instance
         */
        ChatSendMsgRes.create = function create(properties) {
            return new ChatSendMsgRes(properties);
        };

        /**
         * Encodes the specified ChatSendMsgRes message. Does not implicitly {@link chat.ChatSendMsgRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {chat.IChatSendMsgRes} message ChatSendMsgRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatSendMsgRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.msg != null && Object.hasOwnProperty.call(message, "msg"))
                $root.chat.ChatMessage.encode(message.msg, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatSendMsgRes message, length delimited. Does not implicitly {@link chat.ChatSendMsgRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {chat.IChatSendMsgRes} message ChatSendMsgRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatSendMsgRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatSendMsgRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatSendMsgRes} ChatSendMsgRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatSendMsgRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatSendMsgRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.msg = $root.chat.ChatMessage.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatSendMsgRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatSendMsgRes} ChatSendMsgRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatSendMsgRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatSendMsgRes message.
         * @function verify
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatSendMsgRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.msg != null && message.hasOwnProperty("msg")) {
                var error = $root.chat.ChatMessage.verify(message.msg);
                if (error)
                    return "msg." + error;
            }
            return null;
        };

        /**
         * Creates a ChatSendMsgRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatSendMsgRes} ChatSendMsgRes
         */
        ChatSendMsgRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatSendMsgRes)
                return object;
            var message = new $root.chat.ChatSendMsgRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatSendMsgRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.msg != null) {
                if (typeof object.msg !== "object")
                    throw TypeError(".chat.ChatSendMsgRes.msg: object expected");
                message.msg = $root.chat.ChatMessage.fromObject(object.msg);
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatSendMsgRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {chat.ChatSendMsgRes} message ChatSendMsgRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatSendMsgRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                object.msg = null;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = $root.chat.ChatMessage.toObject(message.msg, options);
            return object;
        };

        /**
         * Converts this ChatSendMsgRes to JSON.
         * @function toJSON
         * @memberof chat.ChatSendMsgRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatSendMsgRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatSendMsgRes
         * @function getTypeUrl
         * @memberof chat.ChatSendMsgRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatSendMsgRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatSendMsgRes";
        };

        return ChatSendMsgRes;
    })();

    chat.ChatMsgNotify = (function() {

        /**
         * Properties of a ChatMsgNotify.
         * @memberof chat
         * @interface IChatMsgNotify
         * @property {chat.IChatMessage|null} [msg] ChatMsgNotify msg
         */

        /**
         * Constructs a new ChatMsgNotify.
         * @memberof chat
         * @classdesc Represents a ChatMsgNotify.
         * @implements IChatMsgNotify
         * @constructor
         * @param {chat.IChatMsgNotify=} [properties] Properties to set
         */
        function ChatMsgNotify(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatMsgNotify msg.
         * @member {chat.IChatMessage|null|undefined} msg
         * @memberof chat.ChatMsgNotify
         * @instance
         */
        ChatMsgNotify.prototype.msg = null;

        /**
         * Creates a new ChatMsgNotify instance using the specified properties.
         * @function create
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {chat.IChatMsgNotify=} [properties] Properties to set
         * @returns {chat.ChatMsgNotify} ChatMsgNotify instance
         */
        ChatMsgNotify.create = function create(properties) {
            return new ChatMsgNotify(properties);
        };

        /**
         * Encodes the specified ChatMsgNotify message. Does not implicitly {@link chat.ChatMsgNotify.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {chat.IChatMsgNotify} message ChatMsgNotify message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatMsgNotify.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.msg != null && Object.hasOwnProperty.call(message, "msg"))
                $root.chat.ChatMessage.encode(message.msg, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatMsgNotify message, length delimited. Does not implicitly {@link chat.ChatMsgNotify.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {chat.IChatMsgNotify} message ChatMsgNotify message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatMsgNotify.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatMsgNotify message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatMsgNotify} ChatMsgNotify
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatMsgNotify.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatMsgNotify();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.msg = $root.chat.ChatMessage.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatMsgNotify message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatMsgNotify} ChatMsgNotify
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatMsgNotify.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatMsgNotify message.
         * @function verify
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatMsgNotify.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.msg != null && message.hasOwnProperty("msg")) {
                var error = $root.chat.ChatMessage.verify(message.msg);
                if (error)
                    return "msg." + error;
            }
            return null;
        };

        /**
         * Creates a ChatMsgNotify message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatMsgNotify} ChatMsgNotify
         */
        ChatMsgNotify.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatMsgNotify)
                return object;
            var message = new $root.chat.ChatMsgNotify();
            if (object.msg != null) {
                if (typeof object.msg !== "object")
                    throw TypeError(".chat.ChatMsgNotify.msg: object expected");
                message.msg = $root.chat.ChatMessage.fromObject(object.msg);
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatMsgNotify message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {chat.ChatMsgNotify} message ChatMsgNotify
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatMsgNotify.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.msg = null;
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = $root.chat.ChatMessage.toObject(message.msg, options);
            return object;
        };

        /**
         * Converts this ChatMsgNotify to JSON.
         * @function toJSON
         * @memberof chat.ChatMsgNotify
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatMsgNotify.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatMsgNotify
         * @function getTypeUrl
         * @memberof chat.ChatMsgNotify
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatMsgNotify.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatMsgNotify";
        };

        return ChatMsgNotify;
    })();

    chat.ChatGetHistoryReq = (function() {

        /**
         * Properties of a ChatGetHistoryReq.
         * @memberof chat
         * @interface IChatGetHistoryReq
         * @property {number|Long|null} [roomId] ChatGetHistoryReq roomId
         * @property {number|null} [limit] ChatGetHistoryReq limit
         */

        /**
         * Constructs a new ChatGetHistoryReq.
         * @memberof chat
         * @classdesc Represents a ChatGetHistoryReq.
         * @implements IChatGetHistoryReq
         * @constructor
         * @param {chat.IChatGetHistoryReq=} [properties] Properties to set
         */
        function ChatGetHistoryReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatGetHistoryReq roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatGetHistoryReq
         * @instance
         */
        ChatGetHistoryReq.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatGetHistoryReq limit.
         * @member {number} limit
         * @memberof chat.ChatGetHistoryReq
         * @instance
         */
        ChatGetHistoryReq.prototype.limit = 0;

        /**
         * Creates a new ChatGetHistoryReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {chat.IChatGetHistoryReq=} [properties] Properties to set
         * @returns {chat.ChatGetHistoryReq} ChatGetHistoryReq instance
         */
        ChatGetHistoryReq.create = function create(properties) {
            return new ChatGetHistoryReq(properties);
        };

        /**
         * Encodes the specified ChatGetHistoryReq message. Does not implicitly {@link chat.ChatGetHistoryReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {chat.IChatGetHistoryReq} message ChatGetHistoryReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatGetHistoryReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.limit != null && Object.hasOwnProperty.call(message, "limit"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.limit);
            return writer;
        };

        /**
         * Encodes the specified ChatGetHistoryReq message, length delimited. Does not implicitly {@link chat.ChatGetHistoryReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {chat.IChatGetHistoryReq} message ChatGetHistoryReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatGetHistoryReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatGetHistoryReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatGetHistoryReq} ChatGetHistoryReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatGetHistoryReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatGetHistoryReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.limit = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatGetHistoryReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatGetHistoryReq} ChatGetHistoryReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatGetHistoryReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatGetHistoryReq message.
         * @function verify
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatGetHistoryReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.limit != null && message.hasOwnProperty("limit"))
                if (!$util.isInteger(message.limit))
                    return "limit: integer expected";
            return null;
        };

        /**
         * Creates a ChatGetHistoryReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatGetHistoryReq} ChatGetHistoryReq
         */
        ChatGetHistoryReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatGetHistoryReq)
                return object;
            var message = new $root.chat.ChatGetHistoryReq();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.limit != null)
                message.limit = object.limit >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a ChatGetHistoryReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {chat.ChatGetHistoryReq} message ChatGetHistoryReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatGetHistoryReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                object.limit = 0;
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.limit != null && message.hasOwnProperty("limit"))
                object.limit = message.limit;
            return object;
        };

        /**
         * Converts this ChatGetHistoryReq to JSON.
         * @function toJSON
         * @memberof chat.ChatGetHistoryReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatGetHistoryReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatGetHistoryReq
         * @function getTypeUrl
         * @memberof chat.ChatGetHistoryReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatGetHistoryReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatGetHistoryReq";
        };

        return ChatGetHistoryReq;
    })();

    chat.ChatGetHistoryRes = (function() {

        /**
         * Properties of a ChatGetHistoryRes.
         * @memberof chat
         * @interface IChatGetHistoryRes
         * @property {common.IResult|null} [result] ChatGetHistoryRes result
         * @property {Array.<chat.IChatMessage>|null} [msgs] ChatGetHistoryRes msgs
         */

        /**
         * Constructs a new ChatGetHistoryRes.
         * @memberof chat
         * @classdesc Represents a ChatGetHistoryRes.
         * @implements IChatGetHistoryRes
         * @constructor
         * @param {chat.IChatGetHistoryRes=} [properties] Properties to set
         */
        function ChatGetHistoryRes(properties) {
            this.msgs = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatGetHistoryRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatGetHistoryRes
         * @instance
         */
        ChatGetHistoryRes.prototype.result = null;

        /**
         * ChatGetHistoryRes msgs.
         * @member {Array.<chat.IChatMessage>} msgs
         * @memberof chat.ChatGetHistoryRes
         * @instance
         */
        ChatGetHistoryRes.prototype.msgs = $util.emptyArray;

        /**
         * Creates a new ChatGetHistoryRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {chat.IChatGetHistoryRes=} [properties] Properties to set
         * @returns {chat.ChatGetHistoryRes} ChatGetHistoryRes instance
         */
        ChatGetHistoryRes.create = function create(properties) {
            return new ChatGetHistoryRes(properties);
        };

        /**
         * Encodes the specified ChatGetHistoryRes message. Does not implicitly {@link chat.ChatGetHistoryRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {chat.IChatGetHistoryRes} message ChatGetHistoryRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatGetHistoryRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.msgs != null && message.msgs.length)
                for (var i = 0; i < message.msgs.length; ++i)
                    $root.chat.ChatMessage.encode(message.msgs[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatGetHistoryRes message, length delimited. Does not implicitly {@link chat.ChatGetHistoryRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {chat.IChatGetHistoryRes} message ChatGetHistoryRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatGetHistoryRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatGetHistoryRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatGetHistoryRes} ChatGetHistoryRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatGetHistoryRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatGetHistoryRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        if (!(message.msgs && message.msgs.length))
                            message.msgs = [];
                        message.msgs.push($root.chat.ChatMessage.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatGetHistoryRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatGetHistoryRes} ChatGetHistoryRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatGetHistoryRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatGetHistoryRes message.
         * @function verify
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatGetHistoryRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.msgs != null && message.hasOwnProperty("msgs")) {
                if (!Array.isArray(message.msgs))
                    return "msgs: array expected";
                for (var i = 0; i < message.msgs.length; ++i) {
                    var error = $root.chat.ChatMessage.verify(message.msgs[i]);
                    if (error)
                        return "msgs." + error;
                }
            }
            return null;
        };

        /**
         * Creates a ChatGetHistoryRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatGetHistoryRes} ChatGetHistoryRes
         */
        ChatGetHistoryRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatGetHistoryRes)
                return object;
            var message = new $root.chat.ChatGetHistoryRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatGetHistoryRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.msgs) {
                if (!Array.isArray(object.msgs))
                    throw TypeError(".chat.ChatGetHistoryRes.msgs: array expected");
                message.msgs = [];
                for (var i = 0; i < object.msgs.length; ++i) {
                    if (typeof object.msgs[i] !== "object")
                        throw TypeError(".chat.ChatGetHistoryRes.msgs: object expected");
                    message.msgs[i] = $root.chat.ChatMessage.fromObject(object.msgs[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatGetHistoryRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {chat.ChatGetHistoryRes} message ChatGetHistoryRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatGetHistoryRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.msgs = [];
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.msgs && message.msgs.length) {
                object.msgs = [];
                for (var j = 0; j < message.msgs.length; ++j)
                    object.msgs[j] = $root.chat.ChatMessage.toObject(message.msgs[j], options);
            }
            return object;
        };

        /**
         * Converts this ChatGetHistoryRes to JSON.
         * @function toJSON
         * @memberof chat.ChatGetHistoryRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatGetHistoryRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatGetHistoryRes
         * @function getTypeUrl
         * @memberof chat.ChatGetHistoryRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatGetHistoryRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatGetHistoryRes";
        };

        return ChatGetHistoryRes;
    })();

    chat.ChatListRoomReq = (function() {

        /**
         * Properties of a ChatListRoomReq.
         * @memberof chat
         * @interface IChatListRoomReq
         * @property {number|null} [page] ChatListRoomReq page
         * @property {number|null} [limit] ChatListRoomReq limit
         */

        /**
         * Constructs a new ChatListRoomReq.
         * @memberof chat
         * @classdesc Represents a ChatListRoomReq.
         * @implements IChatListRoomReq
         * @constructor
         * @param {chat.IChatListRoomReq=} [properties] Properties to set
         */
        function ChatListRoomReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatListRoomReq page.
         * @member {number} page
         * @memberof chat.ChatListRoomReq
         * @instance
         */
        ChatListRoomReq.prototype.page = 0;

        /**
         * ChatListRoomReq limit.
         * @member {number} limit
         * @memberof chat.ChatListRoomReq
         * @instance
         */
        ChatListRoomReq.prototype.limit = 0;

        /**
         * Creates a new ChatListRoomReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {chat.IChatListRoomReq=} [properties] Properties to set
         * @returns {chat.ChatListRoomReq} ChatListRoomReq instance
         */
        ChatListRoomReq.create = function create(properties) {
            return new ChatListRoomReq(properties);
        };

        /**
         * Encodes the specified ChatListRoomReq message. Does not implicitly {@link chat.ChatListRoomReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {chat.IChatListRoomReq} message ChatListRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatListRoomReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.page != null && Object.hasOwnProperty.call(message, "page"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.page);
            if (message.limit != null && Object.hasOwnProperty.call(message, "limit"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.limit);
            return writer;
        };

        /**
         * Encodes the specified ChatListRoomReq message, length delimited. Does not implicitly {@link chat.ChatListRoomReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {chat.IChatListRoomReq} message ChatListRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatListRoomReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatListRoomReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatListRoomReq} ChatListRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatListRoomReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatListRoomReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.page = reader.uint32();
                        break;
                    }
                case 2: {
                        message.limit = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatListRoomReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatListRoomReq} ChatListRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatListRoomReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatListRoomReq message.
         * @function verify
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatListRoomReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.page != null && message.hasOwnProperty("page"))
                if (!$util.isInteger(message.page))
                    return "page: integer expected";
            if (message.limit != null && message.hasOwnProperty("limit"))
                if (!$util.isInteger(message.limit))
                    return "limit: integer expected";
            return null;
        };

        /**
         * Creates a ChatListRoomReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatListRoomReq} ChatListRoomReq
         */
        ChatListRoomReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatListRoomReq)
                return object;
            var message = new $root.chat.ChatListRoomReq();
            if (object.page != null)
                message.page = object.page >>> 0;
            if (object.limit != null)
                message.limit = object.limit >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a ChatListRoomReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {chat.ChatListRoomReq} message ChatListRoomReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatListRoomReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.page = 0;
                object.limit = 0;
            }
            if (message.page != null && message.hasOwnProperty("page"))
                object.page = message.page;
            if (message.limit != null && message.hasOwnProperty("limit"))
                object.limit = message.limit;
            return object;
        };

        /**
         * Converts this ChatListRoomReq to JSON.
         * @function toJSON
         * @memberof chat.ChatListRoomReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatListRoomReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatListRoomReq
         * @function getTypeUrl
         * @memberof chat.ChatListRoomReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatListRoomReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatListRoomReq";
        };

        return ChatListRoomReq;
    })();

    chat.ChatListRoomRes = (function() {

        /**
         * Properties of a ChatListRoomRes.
         * @memberof chat
         * @interface IChatListRoomRes
         * @property {common.IResult|null} [result] ChatListRoomRes result
         * @property {Array.<chat.IChatRoomInfo>|null} [rooms] ChatListRoomRes rooms
         * @property {number|null} [total] ChatListRoomRes total
         */

        /**
         * Constructs a new ChatListRoomRes.
         * @memberof chat
         * @classdesc Represents a ChatListRoomRes.
         * @implements IChatListRoomRes
         * @constructor
         * @param {chat.IChatListRoomRes=} [properties] Properties to set
         */
        function ChatListRoomRes(properties) {
            this.rooms = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatListRoomRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatListRoomRes
         * @instance
         */
        ChatListRoomRes.prototype.result = null;

        /**
         * ChatListRoomRes rooms.
         * @member {Array.<chat.IChatRoomInfo>} rooms
         * @memberof chat.ChatListRoomRes
         * @instance
         */
        ChatListRoomRes.prototype.rooms = $util.emptyArray;

        /**
         * ChatListRoomRes total.
         * @member {number} total
         * @memberof chat.ChatListRoomRes
         * @instance
         */
        ChatListRoomRes.prototype.total = 0;

        /**
         * Creates a new ChatListRoomRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {chat.IChatListRoomRes=} [properties] Properties to set
         * @returns {chat.ChatListRoomRes} ChatListRoomRes instance
         */
        ChatListRoomRes.create = function create(properties) {
            return new ChatListRoomRes(properties);
        };

        /**
         * Encodes the specified ChatListRoomRes message. Does not implicitly {@link chat.ChatListRoomRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {chat.IChatListRoomRes} message ChatListRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatListRoomRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.rooms != null && message.rooms.length)
                for (var i = 0; i < message.rooms.length; ++i)
                    $root.chat.ChatRoomInfo.encode(message.rooms[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            if (message.total != null && Object.hasOwnProperty.call(message, "total"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint32(message.total);
            return writer;
        };

        /**
         * Encodes the specified ChatListRoomRes message, length delimited. Does not implicitly {@link chat.ChatListRoomRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {chat.IChatListRoomRes} message ChatListRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatListRoomRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatListRoomRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatListRoomRes} ChatListRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatListRoomRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatListRoomRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        if (!(message.rooms && message.rooms.length))
                            message.rooms = [];
                        message.rooms.push($root.chat.ChatRoomInfo.decode(reader, reader.uint32()));
                        break;
                    }
                case 3: {
                        message.total = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatListRoomRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatListRoomRes} ChatListRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatListRoomRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatListRoomRes message.
         * @function verify
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatListRoomRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.rooms != null && message.hasOwnProperty("rooms")) {
                if (!Array.isArray(message.rooms))
                    return "rooms: array expected";
                for (var i = 0; i < message.rooms.length; ++i) {
                    var error = $root.chat.ChatRoomInfo.verify(message.rooms[i]);
                    if (error)
                        return "rooms." + error;
                }
            }
            if (message.total != null && message.hasOwnProperty("total"))
                if (!$util.isInteger(message.total))
                    return "total: integer expected";
            return null;
        };

        /**
         * Creates a ChatListRoomRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatListRoomRes} ChatListRoomRes
         */
        ChatListRoomRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatListRoomRes)
                return object;
            var message = new $root.chat.ChatListRoomRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatListRoomRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.rooms) {
                if (!Array.isArray(object.rooms))
                    throw TypeError(".chat.ChatListRoomRes.rooms: array expected");
                message.rooms = [];
                for (var i = 0; i < object.rooms.length; ++i) {
                    if (typeof object.rooms[i] !== "object")
                        throw TypeError(".chat.ChatListRoomRes.rooms: object expected");
                    message.rooms[i] = $root.chat.ChatRoomInfo.fromObject(object.rooms[i]);
                }
            }
            if (object.total != null)
                message.total = object.total >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a ChatListRoomRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {chat.ChatListRoomRes} message ChatListRoomRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatListRoomRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.rooms = [];
            if (options.defaults) {
                object.result = null;
                object.total = 0;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.rooms && message.rooms.length) {
                object.rooms = [];
                for (var j = 0; j < message.rooms.length; ++j)
                    object.rooms[j] = $root.chat.ChatRoomInfo.toObject(message.rooms[j], options);
            }
            if (message.total != null && message.hasOwnProperty("total"))
                object.total = message.total;
            return object;
        };

        /**
         * Converts this ChatListRoomRes to JSON.
         * @function toJSON
         * @memberof chat.ChatListRoomRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatListRoomRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatListRoomRes
         * @function getTypeUrl
         * @memberof chat.ChatListRoomRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatListRoomRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatListRoomRes";
        };

        return ChatListRoomRes;
    })();

    chat.ChatCloseRoomReq = (function() {

        /**
         * Properties of a ChatCloseRoomReq.
         * @memberof chat
         * @interface IChatCloseRoomReq
         * @property {number|Long|null} [roomId] ChatCloseRoomReq roomId
         * @property {number|Long|null} [operatorId] ChatCloseRoomReq operatorId
         */

        /**
         * Constructs a new ChatCloseRoomReq.
         * @memberof chat
         * @classdesc Represents a ChatCloseRoomReq.
         * @implements IChatCloseRoomReq
         * @constructor
         * @param {chat.IChatCloseRoomReq=} [properties] Properties to set
         */
        function ChatCloseRoomReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatCloseRoomReq roomId.
         * @member {number|Long} roomId
         * @memberof chat.ChatCloseRoomReq
         * @instance
         */
        ChatCloseRoomReq.prototype.roomId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * ChatCloseRoomReq operatorId.
         * @member {number|Long} operatorId
         * @memberof chat.ChatCloseRoomReq
         * @instance
         */
        ChatCloseRoomReq.prototype.operatorId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new ChatCloseRoomReq instance using the specified properties.
         * @function create
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {chat.IChatCloseRoomReq=} [properties] Properties to set
         * @returns {chat.ChatCloseRoomReq} ChatCloseRoomReq instance
         */
        ChatCloseRoomReq.create = function create(properties) {
            return new ChatCloseRoomReq(properties);
        };

        /**
         * Encodes the specified ChatCloseRoomReq message. Does not implicitly {@link chat.ChatCloseRoomReq.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {chat.IChatCloseRoomReq} message ChatCloseRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCloseRoomReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.roomId != null && Object.hasOwnProperty.call(message, "roomId"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.roomId);
            if (message.operatorId != null && Object.hasOwnProperty.call(message, "operatorId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.operatorId);
            return writer;
        };

        /**
         * Encodes the specified ChatCloseRoomReq message, length delimited. Does not implicitly {@link chat.ChatCloseRoomReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {chat.IChatCloseRoomReq} message ChatCloseRoomReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCloseRoomReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatCloseRoomReq message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatCloseRoomReq} ChatCloseRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCloseRoomReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatCloseRoomReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.roomId = reader.uint64();
                        break;
                    }
                case 2: {
                        message.operatorId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatCloseRoomReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatCloseRoomReq} ChatCloseRoomReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCloseRoomReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatCloseRoomReq message.
         * @function verify
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatCloseRoomReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (!$util.isInteger(message.roomId) && !(message.roomId && $util.isInteger(message.roomId.low) && $util.isInteger(message.roomId.high)))
                    return "roomId: integer|Long expected";
            if (message.operatorId != null && message.hasOwnProperty("operatorId"))
                if (!$util.isInteger(message.operatorId) && !(message.operatorId && $util.isInteger(message.operatorId.low) && $util.isInteger(message.operatorId.high)))
                    return "operatorId: integer|Long expected";
            return null;
        };

        /**
         * Creates a ChatCloseRoomReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatCloseRoomReq} ChatCloseRoomReq
         */
        ChatCloseRoomReq.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatCloseRoomReq)
                return object;
            var message = new $root.chat.ChatCloseRoomReq();
            if (object.roomId != null)
                if ($util.Long)
                    (message.roomId = $util.Long.fromValue(object.roomId)).unsigned = true;
                else if (typeof object.roomId === "string")
                    message.roomId = parseInt(object.roomId, 10);
                else if (typeof object.roomId === "number")
                    message.roomId = object.roomId;
                else if (typeof object.roomId === "object")
                    message.roomId = new $util.LongBits(object.roomId.low >>> 0, object.roomId.high >>> 0).toNumber(true);
            if (object.operatorId != null)
                if ($util.Long)
                    (message.operatorId = $util.Long.fromValue(object.operatorId)).unsigned = true;
                else if (typeof object.operatorId === "string")
                    message.operatorId = parseInt(object.operatorId, 10);
                else if (typeof object.operatorId === "number")
                    message.operatorId = object.operatorId;
                else if (typeof object.operatorId === "object")
                    message.operatorId = new $util.LongBits(object.operatorId.low >>> 0, object.operatorId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a ChatCloseRoomReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {chat.ChatCloseRoomReq} message ChatCloseRoomReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatCloseRoomReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.roomId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.roomId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.operatorId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.operatorId = options.longs === String ? "0" : 0;
            }
            if (message.roomId != null && message.hasOwnProperty("roomId"))
                if (typeof message.roomId === "number")
                    object.roomId = options.longs === String ? String(message.roomId) : message.roomId;
                else
                    object.roomId = options.longs === String ? $util.Long.prototype.toString.call(message.roomId) : options.longs === Number ? new $util.LongBits(message.roomId.low >>> 0, message.roomId.high >>> 0).toNumber(true) : message.roomId;
            if (message.operatorId != null && message.hasOwnProperty("operatorId"))
                if (typeof message.operatorId === "number")
                    object.operatorId = options.longs === String ? String(message.operatorId) : message.operatorId;
                else
                    object.operatorId = options.longs === String ? $util.Long.prototype.toString.call(message.operatorId) : options.longs === Number ? new $util.LongBits(message.operatorId.low >>> 0, message.operatorId.high >>> 0).toNumber(true) : message.operatorId;
            return object;
        };

        /**
         * Converts this ChatCloseRoomReq to JSON.
         * @function toJSON
         * @memberof chat.ChatCloseRoomReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatCloseRoomReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatCloseRoomReq
         * @function getTypeUrl
         * @memberof chat.ChatCloseRoomReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatCloseRoomReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatCloseRoomReq";
        };

        return ChatCloseRoomReq;
    })();

    chat.ChatCloseRoomRes = (function() {

        /**
         * Properties of a ChatCloseRoomRes.
         * @memberof chat
         * @interface IChatCloseRoomRes
         * @property {common.IResult|null} [result] ChatCloseRoomRes result
         */

        /**
         * Constructs a new ChatCloseRoomRes.
         * @memberof chat
         * @classdesc Represents a ChatCloseRoomRes.
         * @implements IChatCloseRoomRes
         * @constructor
         * @param {chat.IChatCloseRoomRes=} [properties] Properties to set
         */
        function ChatCloseRoomRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ChatCloseRoomRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof chat.ChatCloseRoomRes
         * @instance
         */
        ChatCloseRoomRes.prototype.result = null;

        /**
         * Creates a new ChatCloseRoomRes instance using the specified properties.
         * @function create
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {chat.IChatCloseRoomRes=} [properties] Properties to set
         * @returns {chat.ChatCloseRoomRes} ChatCloseRoomRes instance
         */
        ChatCloseRoomRes.create = function create(properties) {
            return new ChatCloseRoomRes(properties);
        };

        /**
         * Encodes the specified ChatCloseRoomRes message. Does not implicitly {@link chat.ChatCloseRoomRes.verify|verify} messages.
         * @function encode
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {chat.IChatCloseRoomRes} message ChatCloseRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCloseRoomRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ChatCloseRoomRes message, length delimited. Does not implicitly {@link chat.ChatCloseRoomRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {chat.IChatCloseRoomRes} message ChatCloseRoomRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ChatCloseRoomRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ChatCloseRoomRes message from the specified reader or buffer.
         * @function decode
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {chat.ChatCloseRoomRes} ChatCloseRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCloseRoomRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.chat.ChatCloseRoomRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ChatCloseRoomRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {chat.ChatCloseRoomRes} ChatCloseRoomRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ChatCloseRoomRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ChatCloseRoomRes message.
         * @function verify
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ChatCloseRoomRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            return null;
        };

        /**
         * Creates a ChatCloseRoomRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {chat.ChatCloseRoomRes} ChatCloseRoomRes
         */
        ChatCloseRoomRes.fromObject = function fromObject(object) {
            if (object instanceof $root.chat.ChatCloseRoomRes)
                return object;
            var message = new $root.chat.ChatCloseRoomRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".chat.ChatCloseRoomRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            return message;
        };

        /**
         * Creates a plain object from a ChatCloseRoomRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {chat.ChatCloseRoomRes} message ChatCloseRoomRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ChatCloseRoomRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.result = null;
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            return object;
        };

        /**
         * Converts this ChatCloseRoomRes to JSON.
         * @function toJSON
         * @memberof chat.ChatCloseRoomRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ChatCloseRoomRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ChatCloseRoomRes
         * @function getTypeUrl
         * @memberof chat.ChatCloseRoomRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ChatCloseRoomRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/chat.ChatCloseRoomRes";
        };

        return ChatCloseRoomRes;
    })();

    return chat;
})();

$root.dbproxy = (function() {

    /**
     * Namespace dbproxy.
     * @exports dbproxy
     * @namespace
     */
    var dbproxy = {};

    dbproxy.MySQLColumn = (function() {

        /**
         * Properties of a MySQLColumn.
         * @memberof dbproxy
         * @interface IMySQLColumn
         * @property {string|null} [name] MySQLColumn name
         * @property {string|null} [value] MySQLColumn value
         */

        /**
         * Constructs a new MySQLColumn.
         * @memberof dbproxy
         * @classdesc Represents a MySQLColumn.
         * @implements IMySQLColumn
         * @constructor
         * @param {dbproxy.IMySQLColumn=} [properties] Properties to set
         */
        function MySQLColumn(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLColumn name.
         * @member {string} name
         * @memberof dbproxy.MySQLColumn
         * @instance
         */
        MySQLColumn.prototype.name = "";

        /**
         * MySQLColumn value.
         * @member {string} value
         * @memberof dbproxy.MySQLColumn
         * @instance
         */
        MySQLColumn.prototype.value = "";

        /**
         * Creates a new MySQLColumn instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {dbproxy.IMySQLColumn=} [properties] Properties to set
         * @returns {dbproxy.MySQLColumn} MySQLColumn instance
         */
        MySQLColumn.create = function create(properties) {
            return new MySQLColumn(properties);
        };

        /**
         * Encodes the specified MySQLColumn message. Does not implicitly {@link dbproxy.MySQLColumn.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {dbproxy.IMySQLColumn} message MySQLColumn message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLColumn.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.name != null && Object.hasOwnProperty.call(message, "name"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.name);
            if (message.value != null && Object.hasOwnProperty.call(message, "value"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.value);
            return writer;
        };

        /**
         * Encodes the specified MySQLColumn message, length delimited. Does not implicitly {@link dbproxy.MySQLColumn.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {dbproxy.IMySQLColumn} message MySQLColumn message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLColumn.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLColumn message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLColumn} MySQLColumn
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLColumn.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLColumn();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.name = reader.string();
                        break;
                    }
                case 2: {
                        message.value = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLColumn message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLColumn} MySQLColumn
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLColumn.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLColumn message.
         * @function verify
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLColumn.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.name != null && message.hasOwnProperty("name"))
                if (!$util.isString(message.name))
                    return "name: string expected";
            if (message.value != null && message.hasOwnProperty("value"))
                if (!$util.isString(message.value))
                    return "value: string expected";
            return null;
        };

        /**
         * Creates a MySQLColumn message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLColumn} MySQLColumn
         */
        MySQLColumn.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLColumn)
                return object;
            var message = new $root.dbproxy.MySQLColumn();
            if (object.name != null)
                message.name = String(object.name);
            if (object.value != null)
                message.value = String(object.value);
            return message;
        };

        /**
         * Creates a plain object from a MySQLColumn message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {dbproxy.MySQLColumn} message MySQLColumn
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLColumn.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.name = "";
                object.value = "";
            }
            if (message.name != null && message.hasOwnProperty("name"))
                object.name = message.name;
            if (message.value != null && message.hasOwnProperty("value"))
                object.value = message.value;
            return object;
        };

        /**
         * Converts this MySQLColumn to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLColumn
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLColumn.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLColumn
         * @function getTypeUrl
         * @memberof dbproxy.MySQLColumn
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLColumn.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLColumn";
        };

        return MySQLColumn;
    })();

    dbproxy.MySQLRow = (function() {

        /**
         * Properties of a MySQLRow.
         * @memberof dbproxy
         * @interface IMySQLRow
         * @property {Array.<dbproxy.IMySQLColumn>|null} [columns] MySQLRow columns
         */

        /**
         * Constructs a new MySQLRow.
         * @memberof dbproxy
         * @classdesc Represents a MySQLRow.
         * @implements IMySQLRow
         * @constructor
         * @param {dbproxy.IMySQLRow=} [properties] Properties to set
         */
        function MySQLRow(properties) {
            this.columns = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLRow columns.
         * @member {Array.<dbproxy.IMySQLColumn>} columns
         * @memberof dbproxy.MySQLRow
         * @instance
         */
        MySQLRow.prototype.columns = $util.emptyArray;

        /**
         * Creates a new MySQLRow instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {dbproxy.IMySQLRow=} [properties] Properties to set
         * @returns {dbproxy.MySQLRow} MySQLRow instance
         */
        MySQLRow.create = function create(properties) {
            return new MySQLRow(properties);
        };

        /**
         * Encodes the specified MySQLRow message. Does not implicitly {@link dbproxy.MySQLRow.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {dbproxy.IMySQLRow} message MySQLRow message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLRow.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.columns != null && message.columns.length)
                for (var i = 0; i < message.columns.length; ++i)
                    $root.dbproxy.MySQLColumn.encode(message.columns[i], writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified MySQLRow message, length delimited. Does not implicitly {@link dbproxy.MySQLRow.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {dbproxy.IMySQLRow} message MySQLRow message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLRow.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLRow message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLRow} MySQLRow
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLRow.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLRow();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        if (!(message.columns && message.columns.length))
                            message.columns = [];
                        message.columns.push($root.dbproxy.MySQLColumn.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLRow message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLRow} MySQLRow
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLRow.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLRow message.
         * @function verify
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLRow.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.columns != null && message.hasOwnProperty("columns")) {
                if (!Array.isArray(message.columns))
                    return "columns: array expected";
                for (var i = 0; i < message.columns.length; ++i) {
                    var error = $root.dbproxy.MySQLColumn.verify(message.columns[i]);
                    if (error)
                        return "columns." + error;
                }
            }
            return null;
        };

        /**
         * Creates a MySQLRow message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLRow} MySQLRow
         */
        MySQLRow.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLRow)
                return object;
            var message = new $root.dbproxy.MySQLRow();
            if (object.columns) {
                if (!Array.isArray(object.columns))
                    throw TypeError(".dbproxy.MySQLRow.columns: array expected");
                message.columns = [];
                for (var i = 0; i < object.columns.length; ++i) {
                    if (typeof object.columns[i] !== "object")
                        throw TypeError(".dbproxy.MySQLRow.columns: object expected");
                    message.columns[i] = $root.dbproxy.MySQLColumn.fromObject(object.columns[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a MySQLRow message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {dbproxy.MySQLRow} message MySQLRow
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLRow.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.columns = [];
            if (message.columns && message.columns.length) {
                object.columns = [];
                for (var j = 0; j < message.columns.length; ++j)
                    object.columns[j] = $root.dbproxy.MySQLColumn.toObject(message.columns[j], options);
            }
            return object;
        };

        /**
         * Converts this MySQLRow to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLRow
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLRow.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLRow
         * @function getTypeUrl
         * @memberof dbproxy.MySQLRow
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLRow.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLRow";
        };

        return MySQLRow;
    })();

    dbproxy.MySQLQueryReq = (function() {

        /**
         * Properties of a MySQLQueryReq.
         * @memberof dbproxy
         * @interface IMySQLQueryReq
         * @property {number|Long|null} [uid] MySQLQueryReq uid
         * @property {string|null} [sql] MySQLQueryReq sql
         * @property {Array.<string>|null} [args] MySQLQueryReq args
         */

        /**
         * Constructs a new MySQLQueryReq.
         * @memberof dbproxy
         * @classdesc Represents a MySQLQueryReq.
         * @implements IMySQLQueryReq
         * @constructor
         * @param {dbproxy.IMySQLQueryReq=} [properties] Properties to set
         */
        function MySQLQueryReq(properties) {
            this.args = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLQueryReq uid.
         * @member {number|Long} uid
         * @memberof dbproxy.MySQLQueryReq
         * @instance
         */
        MySQLQueryReq.prototype.uid = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * MySQLQueryReq sql.
         * @member {string} sql
         * @memberof dbproxy.MySQLQueryReq
         * @instance
         */
        MySQLQueryReq.prototype.sql = "";

        /**
         * MySQLQueryReq args.
         * @member {Array.<string>} args
         * @memberof dbproxy.MySQLQueryReq
         * @instance
         */
        MySQLQueryReq.prototype.args = $util.emptyArray;

        /**
         * Creates a new MySQLQueryReq instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {dbproxy.IMySQLQueryReq=} [properties] Properties to set
         * @returns {dbproxy.MySQLQueryReq} MySQLQueryReq instance
         */
        MySQLQueryReq.create = function create(properties) {
            return new MySQLQueryReq(properties);
        };

        /**
         * Encodes the specified MySQLQueryReq message. Does not implicitly {@link dbproxy.MySQLQueryReq.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {dbproxy.IMySQLQueryReq} message MySQLQueryReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLQueryReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.uid != null && Object.hasOwnProperty.call(message, "uid"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.uid);
            if (message.sql != null && Object.hasOwnProperty.call(message, "sql"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.sql);
            if (message.args != null && message.args.length)
                for (var i = 0; i < message.args.length; ++i)
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.args[i]);
            return writer;
        };

        /**
         * Encodes the specified MySQLQueryReq message, length delimited. Does not implicitly {@link dbproxy.MySQLQueryReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {dbproxy.IMySQLQueryReq} message MySQLQueryReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLQueryReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLQueryReq message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLQueryReq} MySQLQueryReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLQueryReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLQueryReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.uid = reader.uint64();
                        break;
                    }
                case 2: {
                        message.sql = reader.string();
                        break;
                    }
                case 3: {
                        if (!(message.args && message.args.length))
                            message.args = [];
                        message.args.push(reader.string());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLQueryReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLQueryReq} MySQLQueryReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLQueryReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLQueryReq message.
         * @function verify
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLQueryReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.uid != null && message.hasOwnProperty("uid"))
                if (!$util.isInteger(message.uid) && !(message.uid && $util.isInteger(message.uid.low) && $util.isInteger(message.uid.high)))
                    return "uid: integer|Long expected";
            if (message.sql != null && message.hasOwnProperty("sql"))
                if (!$util.isString(message.sql))
                    return "sql: string expected";
            if (message.args != null && message.hasOwnProperty("args")) {
                if (!Array.isArray(message.args))
                    return "args: array expected";
                for (var i = 0; i < message.args.length; ++i)
                    if (!$util.isString(message.args[i]))
                        return "args: string[] expected";
            }
            return null;
        };

        /**
         * Creates a MySQLQueryReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLQueryReq} MySQLQueryReq
         */
        MySQLQueryReq.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLQueryReq)
                return object;
            var message = new $root.dbproxy.MySQLQueryReq();
            if (object.uid != null)
                if ($util.Long)
                    (message.uid = $util.Long.fromValue(object.uid)).unsigned = true;
                else if (typeof object.uid === "string")
                    message.uid = parseInt(object.uid, 10);
                else if (typeof object.uid === "number")
                    message.uid = object.uid;
                else if (typeof object.uid === "object")
                    message.uid = new $util.LongBits(object.uid.low >>> 0, object.uid.high >>> 0).toNumber(true);
            if (object.sql != null)
                message.sql = String(object.sql);
            if (object.args) {
                if (!Array.isArray(object.args))
                    throw TypeError(".dbproxy.MySQLQueryReq.args: array expected");
                message.args = [];
                for (var i = 0; i < object.args.length; ++i)
                    message.args[i] = String(object.args[i]);
            }
            return message;
        };

        /**
         * Creates a plain object from a MySQLQueryReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {dbproxy.MySQLQueryReq} message MySQLQueryReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLQueryReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.args = [];
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.uid = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.uid = options.longs === String ? "0" : 0;
                object.sql = "";
            }
            if (message.uid != null && message.hasOwnProperty("uid"))
                if (typeof message.uid === "number")
                    object.uid = options.longs === String ? String(message.uid) : message.uid;
                else
                    object.uid = options.longs === String ? $util.Long.prototype.toString.call(message.uid) : options.longs === Number ? new $util.LongBits(message.uid.low >>> 0, message.uid.high >>> 0).toNumber(true) : message.uid;
            if (message.sql != null && message.hasOwnProperty("sql"))
                object.sql = message.sql;
            if (message.args && message.args.length) {
                object.args = [];
                for (var j = 0; j < message.args.length; ++j)
                    object.args[j] = message.args[j];
            }
            return object;
        };

        /**
         * Converts this MySQLQueryReq to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLQueryReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLQueryReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLQueryReq
         * @function getTypeUrl
         * @memberof dbproxy.MySQLQueryReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLQueryReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLQueryReq";
        };

        return MySQLQueryReq;
    })();

    dbproxy.MySQLQueryRes = (function() {

        /**
         * Properties of a MySQLQueryRes.
         * @memberof dbproxy
         * @interface IMySQLQueryRes
         * @property {boolean|null} [ok] MySQLQueryRes ok
         * @property {Array.<dbproxy.IMySQLRow>|null} [rows] MySQLQueryRes rows
         * @property {string|null} [error] MySQLQueryRes error
         */

        /**
         * Constructs a new MySQLQueryRes.
         * @memberof dbproxy
         * @classdesc Represents a MySQLQueryRes.
         * @implements IMySQLQueryRes
         * @constructor
         * @param {dbproxy.IMySQLQueryRes=} [properties] Properties to set
         */
        function MySQLQueryRes(properties) {
            this.rows = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLQueryRes ok.
         * @member {boolean} ok
         * @memberof dbproxy.MySQLQueryRes
         * @instance
         */
        MySQLQueryRes.prototype.ok = false;

        /**
         * MySQLQueryRes rows.
         * @member {Array.<dbproxy.IMySQLRow>} rows
         * @memberof dbproxy.MySQLQueryRes
         * @instance
         */
        MySQLQueryRes.prototype.rows = $util.emptyArray;

        /**
         * MySQLQueryRes error.
         * @member {string} error
         * @memberof dbproxy.MySQLQueryRes
         * @instance
         */
        MySQLQueryRes.prototype.error = "";

        /**
         * Creates a new MySQLQueryRes instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {dbproxy.IMySQLQueryRes=} [properties] Properties to set
         * @returns {dbproxy.MySQLQueryRes} MySQLQueryRes instance
         */
        MySQLQueryRes.create = function create(properties) {
            return new MySQLQueryRes(properties);
        };

        /**
         * Encodes the specified MySQLQueryRes message. Does not implicitly {@link dbproxy.MySQLQueryRes.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {dbproxy.IMySQLQueryRes} message MySQLQueryRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLQueryRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.ok != null && Object.hasOwnProperty.call(message, "ok"))
                writer.uint32(/* id 1, wireType 0 =*/8).bool(message.ok);
            if (message.rows != null && message.rows.length)
                for (var i = 0; i < message.rows.length; ++i)
                    $root.dbproxy.MySQLRow.encode(message.rows[i], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            if (message.error != null && Object.hasOwnProperty.call(message, "error"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.error);
            return writer;
        };

        /**
         * Encodes the specified MySQLQueryRes message, length delimited. Does not implicitly {@link dbproxy.MySQLQueryRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {dbproxy.IMySQLQueryRes} message MySQLQueryRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLQueryRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLQueryRes message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLQueryRes} MySQLQueryRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLQueryRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLQueryRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.ok = reader.bool();
                        break;
                    }
                case 2: {
                        if (!(message.rows && message.rows.length))
                            message.rows = [];
                        message.rows.push($root.dbproxy.MySQLRow.decode(reader, reader.uint32()));
                        break;
                    }
                case 3: {
                        message.error = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLQueryRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLQueryRes} MySQLQueryRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLQueryRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLQueryRes message.
         * @function verify
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLQueryRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.ok != null && message.hasOwnProperty("ok"))
                if (typeof message.ok !== "boolean")
                    return "ok: boolean expected";
            if (message.rows != null && message.hasOwnProperty("rows")) {
                if (!Array.isArray(message.rows))
                    return "rows: array expected";
                for (var i = 0; i < message.rows.length; ++i) {
                    var error = $root.dbproxy.MySQLRow.verify(message.rows[i]);
                    if (error)
                        return "rows." + error;
                }
            }
            if (message.error != null && message.hasOwnProperty("error"))
                if (!$util.isString(message.error))
                    return "error: string expected";
            return null;
        };

        /**
         * Creates a MySQLQueryRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLQueryRes} MySQLQueryRes
         */
        MySQLQueryRes.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLQueryRes)
                return object;
            var message = new $root.dbproxy.MySQLQueryRes();
            if (object.ok != null)
                message.ok = Boolean(object.ok);
            if (object.rows) {
                if (!Array.isArray(object.rows))
                    throw TypeError(".dbproxy.MySQLQueryRes.rows: array expected");
                message.rows = [];
                for (var i = 0; i < object.rows.length; ++i) {
                    if (typeof object.rows[i] !== "object")
                        throw TypeError(".dbproxy.MySQLQueryRes.rows: object expected");
                    message.rows[i] = $root.dbproxy.MySQLRow.fromObject(object.rows[i]);
                }
            }
            if (object.error != null)
                message.error = String(object.error);
            return message;
        };

        /**
         * Creates a plain object from a MySQLQueryRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {dbproxy.MySQLQueryRes} message MySQLQueryRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLQueryRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.rows = [];
            if (options.defaults) {
                object.ok = false;
                object.error = "";
            }
            if (message.ok != null && message.hasOwnProperty("ok"))
                object.ok = message.ok;
            if (message.rows && message.rows.length) {
                object.rows = [];
                for (var j = 0; j < message.rows.length; ++j)
                    object.rows[j] = $root.dbproxy.MySQLRow.toObject(message.rows[j], options);
            }
            if (message.error != null && message.hasOwnProperty("error"))
                object.error = message.error;
            return object;
        };

        /**
         * Converts this MySQLQueryRes to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLQueryRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLQueryRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLQueryRes
         * @function getTypeUrl
         * @memberof dbproxy.MySQLQueryRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLQueryRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLQueryRes";
        };

        return MySQLQueryRes;
    })();

    dbproxy.MySQLExecReq = (function() {

        /**
         * Properties of a MySQLExecReq.
         * @memberof dbproxy
         * @interface IMySQLExecReq
         * @property {number|Long|null} [uid] MySQLExecReq uid
         * @property {string|null} [sql] MySQLExecReq sql
         * @property {Array.<string>|null} [args] MySQLExecReq args
         */

        /**
         * Constructs a new MySQLExecReq.
         * @memberof dbproxy
         * @classdesc Represents a MySQLExecReq.
         * @implements IMySQLExecReq
         * @constructor
         * @param {dbproxy.IMySQLExecReq=} [properties] Properties to set
         */
        function MySQLExecReq(properties) {
            this.args = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLExecReq uid.
         * @member {number|Long} uid
         * @memberof dbproxy.MySQLExecReq
         * @instance
         */
        MySQLExecReq.prototype.uid = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * MySQLExecReq sql.
         * @member {string} sql
         * @memberof dbproxy.MySQLExecReq
         * @instance
         */
        MySQLExecReq.prototype.sql = "";

        /**
         * MySQLExecReq args.
         * @member {Array.<string>} args
         * @memberof dbproxy.MySQLExecReq
         * @instance
         */
        MySQLExecReq.prototype.args = $util.emptyArray;

        /**
         * Creates a new MySQLExecReq instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {dbproxy.IMySQLExecReq=} [properties] Properties to set
         * @returns {dbproxy.MySQLExecReq} MySQLExecReq instance
         */
        MySQLExecReq.create = function create(properties) {
            return new MySQLExecReq(properties);
        };

        /**
         * Encodes the specified MySQLExecReq message. Does not implicitly {@link dbproxy.MySQLExecReq.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {dbproxy.IMySQLExecReq} message MySQLExecReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLExecReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.uid != null && Object.hasOwnProperty.call(message, "uid"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint64(message.uid);
            if (message.sql != null && Object.hasOwnProperty.call(message, "sql"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.sql);
            if (message.args != null && message.args.length)
                for (var i = 0; i < message.args.length; ++i)
                    writer.uint32(/* id 3, wireType 2 =*/26).string(message.args[i]);
            return writer;
        };

        /**
         * Encodes the specified MySQLExecReq message, length delimited. Does not implicitly {@link dbproxy.MySQLExecReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {dbproxy.IMySQLExecReq} message MySQLExecReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLExecReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLExecReq message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLExecReq} MySQLExecReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLExecReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLExecReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.uid = reader.uint64();
                        break;
                    }
                case 2: {
                        message.sql = reader.string();
                        break;
                    }
                case 3: {
                        if (!(message.args && message.args.length))
                            message.args = [];
                        message.args.push(reader.string());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLExecReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLExecReq} MySQLExecReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLExecReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLExecReq message.
         * @function verify
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLExecReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.uid != null && message.hasOwnProperty("uid"))
                if (!$util.isInteger(message.uid) && !(message.uid && $util.isInteger(message.uid.low) && $util.isInteger(message.uid.high)))
                    return "uid: integer|Long expected";
            if (message.sql != null && message.hasOwnProperty("sql"))
                if (!$util.isString(message.sql))
                    return "sql: string expected";
            if (message.args != null && message.hasOwnProperty("args")) {
                if (!Array.isArray(message.args))
                    return "args: array expected";
                for (var i = 0; i < message.args.length; ++i)
                    if (!$util.isString(message.args[i]))
                        return "args: string[] expected";
            }
            return null;
        };

        /**
         * Creates a MySQLExecReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLExecReq} MySQLExecReq
         */
        MySQLExecReq.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLExecReq)
                return object;
            var message = new $root.dbproxy.MySQLExecReq();
            if (object.uid != null)
                if ($util.Long)
                    (message.uid = $util.Long.fromValue(object.uid)).unsigned = true;
                else if (typeof object.uid === "string")
                    message.uid = parseInt(object.uid, 10);
                else if (typeof object.uid === "number")
                    message.uid = object.uid;
                else if (typeof object.uid === "object")
                    message.uid = new $util.LongBits(object.uid.low >>> 0, object.uid.high >>> 0).toNumber(true);
            if (object.sql != null)
                message.sql = String(object.sql);
            if (object.args) {
                if (!Array.isArray(object.args))
                    throw TypeError(".dbproxy.MySQLExecReq.args: array expected");
                message.args = [];
                for (var i = 0; i < object.args.length; ++i)
                    message.args[i] = String(object.args[i]);
            }
            return message;
        };

        /**
         * Creates a plain object from a MySQLExecReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {dbproxy.MySQLExecReq} message MySQLExecReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLExecReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.args = [];
            if (options.defaults) {
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.uid = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.uid = options.longs === String ? "0" : 0;
                object.sql = "";
            }
            if (message.uid != null && message.hasOwnProperty("uid"))
                if (typeof message.uid === "number")
                    object.uid = options.longs === String ? String(message.uid) : message.uid;
                else
                    object.uid = options.longs === String ? $util.Long.prototype.toString.call(message.uid) : options.longs === Number ? new $util.LongBits(message.uid.low >>> 0, message.uid.high >>> 0).toNumber(true) : message.uid;
            if (message.sql != null && message.hasOwnProperty("sql"))
                object.sql = message.sql;
            if (message.args && message.args.length) {
                object.args = [];
                for (var j = 0; j < message.args.length; ++j)
                    object.args[j] = message.args[j];
            }
            return object;
        };

        /**
         * Converts this MySQLExecReq to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLExecReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLExecReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLExecReq
         * @function getTypeUrl
         * @memberof dbproxy.MySQLExecReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLExecReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLExecReq";
        };

        return MySQLExecReq;
    })();

    dbproxy.MySQLExecRes = (function() {

        /**
         * Properties of a MySQLExecRes.
         * @memberof dbproxy
         * @interface IMySQLExecRes
         * @property {boolean|null} [ok] MySQLExecRes ok
         * @property {number|Long|null} [lastInsertId] MySQLExecRes lastInsertId
         * @property {number|Long|null} [rowsAffected] MySQLExecRes rowsAffected
         * @property {string|null} [error] MySQLExecRes error
         */

        /**
         * Constructs a new MySQLExecRes.
         * @memberof dbproxy
         * @classdesc Represents a MySQLExecRes.
         * @implements IMySQLExecRes
         * @constructor
         * @param {dbproxy.IMySQLExecRes=} [properties] Properties to set
         */
        function MySQLExecRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * MySQLExecRes ok.
         * @member {boolean} ok
         * @memberof dbproxy.MySQLExecRes
         * @instance
         */
        MySQLExecRes.prototype.ok = false;

        /**
         * MySQLExecRes lastInsertId.
         * @member {number|Long} lastInsertId
         * @memberof dbproxy.MySQLExecRes
         * @instance
         */
        MySQLExecRes.prototype.lastInsertId = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

        /**
         * MySQLExecRes rowsAffected.
         * @member {number|Long} rowsAffected
         * @memberof dbproxy.MySQLExecRes
         * @instance
         */
        MySQLExecRes.prototype.rowsAffected = $util.Long ? $util.Long.fromBits(0,0,false) : 0;

        /**
         * MySQLExecRes error.
         * @member {string} error
         * @memberof dbproxy.MySQLExecRes
         * @instance
         */
        MySQLExecRes.prototype.error = "";

        /**
         * Creates a new MySQLExecRes instance using the specified properties.
         * @function create
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {dbproxy.IMySQLExecRes=} [properties] Properties to set
         * @returns {dbproxy.MySQLExecRes} MySQLExecRes instance
         */
        MySQLExecRes.create = function create(properties) {
            return new MySQLExecRes(properties);
        };

        /**
         * Encodes the specified MySQLExecRes message. Does not implicitly {@link dbproxy.MySQLExecRes.verify|verify} messages.
         * @function encode
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {dbproxy.IMySQLExecRes} message MySQLExecRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLExecRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.ok != null && Object.hasOwnProperty.call(message, "ok"))
                writer.uint32(/* id 1, wireType 0 =*/8).bool(message.ok);
            if (message.lastInsertId != null && Object.hasOwnProperty.call(message, "lastInsertId"))
                writer.uint32(/* id 2, wireType 0 =*/16).int64(message.lastInsertId);
            if (message.rowsAffected != null && Object.hasOwnProperty.call(message, "rowsAffected"))
                writer.uint32(/* id 3, wireType 0 =*/24).int64(message.rowsAffected);
            if (message.error != null && Object.hasOwnProperty.call(message, "error"))
                writer.uint32(/* id 4, wireType 2 =*/34).string(message.error);
            return writer;
        };

        /**
         * Encodes the specified MySQLExecRes message, length delimited. Does not implicitly {@link dbproxy.MySQLExecRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {dbproxy.IMySQLExecRes} message MySQLExecRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        MySQLExecRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a MySQLExecRes message from the specified reader or buffer.
         * @function decode
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {dbproxy.MySQLExecRes} MySQLExecRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLExecRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.dbproxy.MySQLExecRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.ok = reader.bool();
                        break;
                    }
                case 2: {
                        message.lastInsertId = reader.int64();
                        break;
                    }
                case 3: {
                        message.rowsAffected = reader.int64();
                        break;
                    }
                case 4: {
                        message.error = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a MySQLExecRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {dbproxy.MySQLExecRes} MySQLExecRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        MySQLExecRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a MySQLExecRes message.
         * @function verify
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        MySQLExecRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.ok != null && message.hasOwnProperty("ok"))
                if (typeof message.ok !== "boolean")
                    return "ok: boolean expected";
            if (message.lastInsertId != null && message.hasOwnProperty("lastInsertId"))
                if (!$util.isInteger(message.lastInsertId) && !(message.lastInsertId && $util.isInteger(message.lastInsertId.low) && $util.isInteger(message.lastInsertId.high)))
                    return "lastInsertId: integer|Long expected";
            if (message.rowsAffected != null && message.hasOwnProperty("rowsAffected"))
                if (!$util.isInteger(message.rowsAffected) && !(message.rowsAffected && $util.isInteger(message.rowsAffected.low) && $util.isInteger(message.rowsAffected.high)))
                    return "rowsAffected: integer|Long expected";
            if (message.error != null && message.hasOwnProperty("error"))
                if (!$util.isString(message.error))
                    return "error: string expected";
            return null;
        };

        /**
         * Creates a MySQLExecRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {dbproxy.MySQLExecRes} MySQLExecRes
         */
        MySQLExecRes.fromObject = function fromObject(object) {
            if (object instanceof $root.dbproxy.MySQLExecRes)
                return object;
            var message = new $root.dbproxy.MySQLExecRes();
            if (object.ok != null)
                message.ok = Boolean(object.ok);
            if (object.lastInsertId != null)
                if ($util.Long)
                    (message.lastInsertId = $util.Long.fromValue(object.lastInsertId)).unsigned = false;
                else if (typeof object.lastInsertId === "string")
                    message.lastInsertId = parseInt(object.lastInsertId, 10);
                else if (typeof object.lastInsertId === "number")
                    message.lastInsertId = object.lastInsertId;
                else if (typeof object.lastInsertId === "object")
                    message.lastInsertId = new $util.LongBits(object.lastInsertId.low >>> 0, object.lastInsertId.high >>> 0).toNumber();
            if (object.rowsAffected != null)
                if ($util.Long)
                    (message.rowsAffected = $util.Long.fromValue(object.rowsAffected)).unsigned = false;
                else if (typeof object.rowsAffected === "string")
                    message.rowsAffected = parseInt(object.rowsAffected, 10);
                else if (typeof object.rowsAffected === "number")
                    message.rowsAffected = object.rowsAffected;
                else if (typeof object.rowsAffected === "object")
                    message.rowsAffected = new $util.LongBits(object.rowsAffected.low >>> 0, object.rowsAffected.high >>> 0).toNumber();
            if (object.error != null)
                message.error = String(object.error);
            return message;
        };

        /**
         * Creates a plain object from a MySQLExecRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {dbproxy.MySQLExecRes} message MySQLExecRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        MySQLExecRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.ok = false;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, false);
                    object.lastInsertId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.lastInsertId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, false);
                    object.rowsAffected = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.rowsAffected = options.longs === String ? "0" : 0;
                object.error = "";
            }
            if (message.ok != null && message.hasOwnProperty("ok"))
                object.ok = message.ok;
            if (message.lastInsertId != null && message.hasOwnProperty("lastInsertId"))
                if (typeof message.lastInsertId === "number")
                    object.lastInsertId = options.longs === String ? String(message.lastInsertId) : message.lastInsertId;
                else
                    object.lastInsertId = options.longs === String ? $util.Long.prototype.toString.call(message.lastInsertId) : options.longs === Number ? new $util.LongBits(message.lastInsertId.low >>> 0, message.lastInsertId.high >>> 0).toNumber() : message.lastInsertId;
            if (message.rowsAffected != null && message.hasOwnProperty("rowsAffected"))
                if (typeof message.rowsAffected === "number")
                    object.rowsAffected = options.longs === String ? String(message.rowsAffected) : message.rowsAffected;
                else
                    object.rowsAffected = options.longs === String ? $util.Long.prototype.toString.call(message.rowsAffected) : options.longs === Number ? new $util.LongBits(message.rowsAffected.low >>> 0, message.rowsAffected.high >>> 0).toNumber() : message.rowsAffected;
            if (message.error != null && message.hasOwnProperty("error"))
                object.error = message.error;
            return object;
        };

        /**
         * Converts this MySQLExecRes to JSON.
         * @function toJSON
         * @memberof dbproxy.MySQLExecRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        MySQLExecRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for MySQLExecRes
         * @function getTypeUrl
         * @memberof dbproxy.MySQLExecRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        MySQLExecRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/dbproxy.MySQLExecRes";
        };

        return MySQLExecRes;
    })();

    return dbproxy;
})();

$root.login = (function() {

    /**
     * Namespace login.
     * @exports login
     * @namespace
     */
    var login = {};

    login.LoginReq = (function() {

        /**
         * Properties of a LoginReq.
         * @memberof login
         * @interface ILoginReq
         * @property {string|null} [account] LoginReq account
         * @property {string|null} [password] LoginReq password
         * @property {string|null} [platform] LoginReq platform
         * @property {string|null} [version] LoginReq version
         */

        /**
         * Constructs a new LoginReq.
         * @memberof login
         * @classdesc Represents a LoginReq.
         * @implements ILoginReq
         * @constructor
         * @param {login.ILoginReq=} [properties] Properties to set
         */
        function LoginReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * LoginReq account.
         * @member {string} account
         * @memberof login.LoginReq
         * @instance
         */
        LoginReq.prototype.account = "";

        /**
         * LoginReq password.
         * @member {string} password
         * @memberof login.LoginReq
         * @instance
         */
        LoginReq.prototype.password = "";

        /**
         * LoginReq platform.
         * @member {string} platform
         * @memberof login.LoginReq
         * @instance
         */
        LoginReq.prototype.platform = "";

        /**
         * LoginReq version.
         * @member {string} version
         * @memberof login.LoginReq
         * @instance
         */
        LoginReq.prototype.version = "";

        /**
         * Creates a new LoginReq instance using the specified properties.
         * @function create
         * @memberof login.LoginReq
         * @static
         * @param {login.ILoginReq=} [properties] Properties to set
         * @returns {login.LoginReq} LoginReq instance
         */
        LoginReq.create = function create(properties) {
            return new LoginReq(properties);
        };

        /**
         * Encodes the specified LoginReq message. Does not implicitly {@link login.LoginReq.verify|verify} messages.
         * @function encode
         * @memberof login.LoginReq
         * @static
         * @param {login.ILoginReq} message LoginReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        LoginReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.account != null && Object.hasOwnProperty.call(message, "account"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.account);
            if (message.password != null && Object.hasOwnProperty.call(message, "password"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.password);
            if (message.platform != null && Object.hasOwnProperty.call(message, "platform"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.platform);
            if (message.version != null && Object.hasOwnProperty.call(message, "version"))
                writer.uint32(/* id 4, wireType 2 =*/34).string(message.version);
            return writer;
        };

        /**
         * Encodes the specified LoginReq message, length delimited. Does not implicitly {@link login.LoginReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.LoginReq
         * @static
         * @param {login.ILoginReq} message LoginReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        LoginReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a LoginReq message from the specified reader or buffer.
         * @function decode
         * @memberof login.LoginReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.LoginReq} LoginReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        LoginReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.LoginReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.account = reader.string();
                        break;
                    }
                case 2: {
                        message.password = reader.string();
                        break;
                    }
                case 3: {
                        message.platform = reader.string();
                        break;
                    }
                case 4: {
                        message.version = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a LoginReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.LoginReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.LoginReq} LoginReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        LoginReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a LoginReq message.
         * @function verify
         * @memberof login.LoginReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        LoginReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.account != null && message.hasOwnProperty("account"))
                if (!$util.isString(message.account))
                    return "account: string expected";
            if (message.password != null && message.hasOwnProperty("password"))
                if (!$util.isString(message.password))
                    return "password: string expected";
            if (message.platform != null && message.hasOwnProperty("platform"))
                if (!$util.isString(message.platform))
                    return "platform: string expected";
            if (message.version != null && message.hasOwnProperty("version"))
                if (!$util.isString(message.version))
                    return "version: string expected";
            return null;
        };

        /**
         * Creates a LoginReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.LoginReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.LoginReq} LoginReq
         */
        LoginReq.fromObject = function fromObject(object) {
            if (object instanceof $root.login.LoginReq)
                return object;
            var message = new $root.login.LoginReq();
            if (object.account != null)
                message.account = String(object.account);
            if (object.password != null)
                message.password = String(object.password);
            if (object.platform != null)
                message.platform = String(object.platform);
            if (object.version != null)
                message.version = String(object.version);
            return message;
        };

        /**
         * Creates a plain object from a LoginReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.LoginReq
         * @static
         * @param {login.LoginReq} message LoginReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        LoginReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.account = "";
                object.password = "";
                object.platform = "";
                object.version = "";
            }
            if (message.account != null && message.hasOwnProperty("account"))
                object.account = message.account;
            if (message.password != null && message.hasOwnProperty("password"))
                object.password = message.password;
            if (message.platform != null && message.hasOwnProperty("platform"))
                object.platform = message.platform;
            if (message.version != null && message.hasOwnProperty("version"))
                object.version = message.version;
            return object;
        };

        /**
         * Converts this LoginReq to JSON.
         * @function toJSON
         * @memberof login.LoginReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        LoginReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for LoginReq
         * @function getTypeUrl
         * @memberof login.LoginReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        LoginReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.LoginReq";
        };

        return LoginReq;
    })();

    login.LoginRes = (function() {

        /**
         * Properties of a LoginRes.
         * @memberof login
         * @interface ILoginRes
         * @property {common.IResult|null} [result] LoginRes result
         * @property {string|null} [token] LoginRes token
         * @property {number|Long|null} [playerId] LoginRes playerId
         * @property {number|Long|null} [expireAt] LoginRes expireAt
         */

        /**
         * Constructs a new LoginRes.
         * @memberof login
         * @classdesc Represents a LoginRes.
         * @implements ILoginRes
         * @constructor
         * @param {login.ILoginRes=} [properties] Properties to set
         */
        function LoginRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * LoginRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof login.LoginRes
         * @instance
         */
        LoginRes.prototype.result = null;

        /**
         * LoginRes token.
         * @member {string} token
         * @memberof login.LoginRes
         * @instance
         */
        LoginRes.prototype.token = "";

        /**
         * LoginRes playerId.
         * @member {number|Long} playerId
         * @memberof login.LoginRes
         * @instance
         */
        LoginRes.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * LoginRes expireAt.
         * @member {number|Long} expireAt
         * @memberof login.LoginRes
         * @instance
         */
        LoginRes.prototype.expireAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new LoginRes instance using the specified properties.
         * @function create
         * @memberof login.LoginRes
         * @static
         * @param {login.ILoginRes=} [properties] Properties to set
         * @returns {login.LoginRes} LoginRes instance
         */
        LoginRes.create = function create(properties) {
            return new LoginRes(properties);
        };

        /**
         * Encodes the specified LoginRes message. Does not implicitly {@link login.LoginRes.verify|verify} messages.
         * @function encode
         * @memberof login.LoginRes
         * @static
         * @param {login.ILoginRes} message LoginRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        LoginRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.token != null && Object.hasOwnProperty.call(message, "token"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.token);
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint64(message.playerId);
            if (message.expireAt != null && Object.hasOwnProperty.call(message, "expireAt"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint64(message.expireAt);
            return writer;
        };

        /**
         * Encodes the specified LoginRes message, length delimited. Does not implicitly {@link login.LoginRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.LoginRes
         * @static
         * @param {login.ILoginRes} message LoginRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        LoginRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a LoginRes message from the specified reader or buffer.
         * @function decode
         * @memberof login.LoginRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.LoginRes} LoginRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        LoginRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.LoginRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.token = reader.string();
                        break;
                    }
                case 3: {
                        message.playerId = reader.uint64();
                        break;
                    }
                case 4: {
                        message.expireAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a LoginRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.LoginRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.LoginRes} LoginRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        LoginRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a LoginRes message.
         * @function verify
         * @memberof login.LoginRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        LoginRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.token != null && message.hasOwnProperty("token"))
                if (!$util.isString(message.token))
                    return "token: string expected";
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            if (message.expireAt != null && message.hasOwnProperty("expireAt"))
                if (!$util.isInteger(message.expireAt) && !(message.expireAt && $util.isInteger(message.expireAt.low) && $util.isInteger(message.expireAt.high)))
                    return "expireAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a LoginRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.LoginRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.LoginRes} LoginRes
         */
        LoginRes.fromObject = function fromObject(object) {
            if (object instanceof $root.login.LoginRes)
                return object;
            var message = new $root.login.LoginRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".login.LoginRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.token != null)
                message.token = String(object.token);
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            if (object.expireAt != null)
                if ($util.Long)
                    (message.expireAt = $util.Long.fromValue(object.expireAt)).unsigned = true;
                else if (typeof object.expireAt === "string")
                    message.expireAt = parseInt(object.expireAt, 10);
                else if (typeof object.expireAt === "number")
                    message.expireAt = object.expireAt;
                else if (typeof object.expireAt === "object")
                    message.expireAt = new $util.LongBits(object.expireAt.low >>> 0, object.expireAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a LoginRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.LoginRes
         * @static
         * @param {login.LoginRes} message LoginRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        LoginRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                object.token = "";
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.expireAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.expireAt = options.longs === String ? "0" : 0;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.token != null && message.hasOwnProperty("token"))
                object.token = message.token;
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            if (message.expireAt != null && message.hasOwnProperty("expireAt"))
                if (typeof message.expireAt === "number")
                    object.expireAt = options.longs === String ? String(message.expireAt) : message.expireAt;
                else
                    object.expireAt = options.longs === String ? $util.Long.prototype.toString.call(message.expireAt) : options.longs === Number ? new $util.LongBits(message.expireAt.low >>> 0, message.expireAt.high >>> 0).toNumber(true) : message.expireAt;
            return object;
        };

        /**
         * Converts this LoginRes to JSON.
         * @function toJSON
         * @memberof login.LoginRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        LoginRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for LoginRes
         * @function getTypeUrl
         * @memberof login.LoginRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        LoginRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.LoginRes";
        };

        return LoginRes;
    })();

    login.RegisterReq = (function() {

        /**
         * Properties of a RegisterReq.
         * @memberof login
         * @interface IRegisterReq
         * @property {string|null} [account] RegisterReq account
         * @property {string|null} [password] RegisterReq password
         * @property {string|null} [platform] RegisterReq platform
         */

        /**
         * Constructs a new RegisterReq.
         * @memberof login
         * @classdesc Represents a RegisterReq.
         * @implements IRegisterReq
         * @constructor
         * @param {login.IRegisterReq=} [properties] Properties to set
         */
        function RegisterReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * RegisterReq account.
         * @member {string} account
         * @memberof login.RegisterReq
         * @instance
         */
        RegisterReq.prototype.account = "";

        /**
         * RegisterReq password.
         * @member {string} password
         * @memberof login.RegisterReq
         * @instance
         */
        RegisterReq.prototype.password = "";

        /**
         * RegisterReq platform.
         * @member {string} platform
         * @memberof login.RegisterReq
         * @instance
         */
        RegisterReq.prototype.platform = "";

        /**
         * Creates a new RegisterReq instance using the specified properties.
         * @function create
         * @memberof login.RegisterReq
         * @static
         * @param {login.IRegisterReq=} [properties] Properties to set
         * @returns {login.RegisterReq} RegisterReq instance
         */
        RegisterReq.create = function create(properties) {
            return new RegisterReq(properties);
        };

        /**
         * Encodes the specified RegisterReq message. Does not implicitly {@link login.RegisterReq.verify|verify} messages.
         * @function encode
         * @memberof login.RegisterReq
         * @static
         * @param {login.IRegisterReq} message RegisterReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        RegisterReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.account != null && Object.hasOwnProperty.call(message, "account"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.account);
            if (message.password != null && Object.hasOwnProperty.call(message, "password"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.password);
            if (message.platform != null && Object.hasOwnProperty.call(message, "platform"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.platform);
            return writer;
        };

        /**
         * Encodes the specified RegisterReq message, length delimited. Does not implicitly {@link login.RegisterReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.RegisterReq
         * @static
         * @param {login.IRegisterReq} message RegisterReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        RegisterReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a RegisterReq message from the specified reader or buffer.
         * @function decode
         * @memberof login.RegisterReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.RegisterReq} RegisterReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        RegisterReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.RegisterReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.account = reader.string();
                        break;
                    }
                case 2: {
                        message.password = reader.string();
                        break;
                    }
                case 3: {
                        message.platform = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a RegisterReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.RegisterReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.RegisterReq} RegisterReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        RegisterReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a RegisterReq message.
         * @function verify
         * @memberof login.RegisterReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        RegisterReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.account != null && message.hasOwnProperty("account"))
                if (!$util.isString(message.account))
                    return "account: string expected";
            if (message.password != null && message.hasOwnProperty("password"))
                if (!$util.isString(message.password))
                    return "password: string expected";
            if (message.platform != null && message.hasOwnProperty("platform"))
                if (!$util.isString(message.platform))
                    return "platform: string expected";
            return null;
        };

        /**
         * Creates a RegisterReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.RegisterReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.RegisterReq} RegisterReq
         */
        RegisterReq.fromObject = function fromObject(object) {
            if (object instanceof $root.login.RegisterReq)
                return object;
            var message = new $root.login.RegisterReq();
            if (object.account != null)
                message.account = String(object.account);
            if (object.password != null)
                message.password = String(object.password);
            if (object.platform != null)
                message.platform = String(object.platform);
            return message;
        };

        /**
         * Creates a plain object from a RegisterReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.RegisterReq
         * @static
         * @param {login.RegisterReq} message RegisterReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        RegisterReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.account = "";
                object.password = "";
                object.platform = "";
            }
            if (message.account != null && message.hasOwnProperty("account"))
                object.account = message.account;
            if (message.password != null && message.hasOwnProperty("password"))
                object.password = message.password;
            if (message.platform != null && message.hasOwnProperty("platform"))
                object.platform = message.platform;
            return object;
        };

        /**
         * Converts this RegisterReq to JSON.
         * @function toJSON
         * @memberof login.RegisterReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        RegisterReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for RegisterReq
         * @function getTypeUrl
         * @memberof login.RegisterReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        RegisterReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.RegisterReq";
        };

        return RegisterReq;
    })();

    login.RegisterRes = (function() {

        /**
         * Properties of a RegisterRes.
         * @memberof login
         * @interface IRegisterRes
         * @property {common.IResult|null} [result] RegisterRes result
         * @property {number|Long|null} [playerId] RegisterRes playerId
         */

        /**
         * Constructs a new RegisterRes.
         * @memberof login
         * @classdesc Represents a RegisterRes.
         * @implements IRegisterRes
         * @constructor
         * @param {login.IRegisterRes=} [properties] Properties to set
         */
        function RegisterRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * RegisterRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof login.RegisterRes
         * @instance
         */
        RegisterRes.prototype.result = null;

        /**
         * RegisterRes playerId.
         * @member {number|Long} playerId
         * @memberof login.RegisterRes
         * @instance
         */
        RegisterRes.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new RegisterRes instance using the specified properties.
         * @function create
         * @memberof login.RegisterRes
         * @static
         * @param {login.IRegisterRes=} [properties] Properties to set
         * @returns {login.RegisterRes} RegisterRes instance
         */
        RegisterRes.create = function create(properties) {
            return new RegisterRes(properties);
        };

        /**
         * Encodes the specified RegisterRes message. Does not implicitly {@link login.RegisterRes.verify|verify} messages.
         * @function encode
         * @memberof login.RegisterRes
         * @static
         * @param {login.IRegisterRes} message RegisterRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        RegisterRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.playerId);
            return writer;
        };

        /**
         * Encodes the specified RegisterRes message, length delimited. Does not implicitly {@link login.RegisterRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.RegisterRes
         * @static
         * @param {login.IRegisterRes} message RegisterRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        RegisterRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a RegisterRes message from the specified reader or buffer.
         * @function decode
         * @memberof login.RegisterRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.RegisterRes} RegisterRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        RegisterRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.RegisterRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.playerId = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a RegisterRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.RegisterRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.RegisterRes} RegisterRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        RegisterRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a RegisterRes message.
         * @function verify
         * @memberof login.RegisterRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        RegisterRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            return null;
        };

        /**
         * Creates a RegisterRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.RegisterRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.RegisterRes} RegisterRes
         */
        RegisterRes.fromObject = function fromObject(object) {
            if (object instanceof $root.login.RegisterRes)
                return object;
            var message = new $root.login.RegisterRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".login.RegisterRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a RegisterRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.RegisterRes
         * @static
         * @param {login.RegisterRes} message RegisterRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        RegisterRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            return object;
        };

        /**
         * Converts this RegisterRes to JSON.
         * @function toJSON
         * @memberof login.RegisterRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        RegisterRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for RegisterRes
         * @function getTypeUrl
         * @memberof login.RegisterRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        RegisterRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.RegisterRes";
        };

        return RegisterRes;
    })();

    login.VerifyTokenReq = (function() {

        /**
         * Properties of a VerifyTokenReq.
         * @memberof login
         * @interface IVerifyTokenReq
         * @property {string|null} [token] VerifyTokenReq token
         */

        /**
         * Constructs a new VerifyTokenReq.
         * @memberof login
         * @classdesc Represents a VerifyTokenReq.
         * @implements IVerifyTokenReq
         * @constructor
         * @param {login.IVerifyTokenReq=} [properties] Properties to set
         */
        function VerifyTokenReq(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * VerifyTokenReq token.
         * @member {string} token
         * @memberof login.VerifyTokenReq
         * @instance
         */
        VerifyTokenReq.prototype.token = "";

        /**
         * Creates a new VerifyTokenReq instance using the specified properties.
         * @function create
         * @memberof login.VerifyTokenReq
         * @static
         * @param {login.IVerifyTokenReq=} [properties] Properties to set
         * @returns {login.VerifyTokenReq} VerifyTokenReq instance
         */
        VerifyTokenReq.create = function create(properties) {
            return new VerifyTokenReq(properties);
        };

        /**
         * Encodes the specified VerifyTokenReq message. Does not implicitly {@link login.VerifyTokenReq.verify|verify} messages.
         * @function encode
         * @memberof login.VerifyTokenReq
         * @static
         * @param {login.IVerifyTokenReq} message VerifyTokenReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        VerifyTokenReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.token != null && Object.hasOwnProperty.call(message, "token"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.token);
            return writer;
        };

        /**
         * Encodes the specified VerifyTokenReq message, length delimited. Does not implicitly {@link login.VerifyTokenReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.VerifyTokenReq
         * @static
         * @param {login.IVerifyTokenReq} message VerifyTokenReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        VerifyTokenReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a VerifyTokenReq message from the specified reader or buffer.
         * @function decode
         * @memberof login.VerifyTokenReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.VerifyTokenReq} VerifyTokenReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        VerifyTokenReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.VerifyTokenReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.token = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a VerifyTokenReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.VerifyTokenReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.VerifyTokenReq} VerifyTokenReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        VerifyTokenReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a VerifyTokenReq message.
         * @function verify
         * @memberof login.VerifyTokenReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        VerifyTokenReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.token != null && message.hasOwnProperty("token"))
                if (!$util.isString(message.token))
                    return "token: string expected";
            return null;
        };

        /**
         * Creates a VerifyTokenReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.VerifyTokenReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.VerifyTokenReq} VerifyTokenReq
         */
        VerifyTokenReq.fromObject = function fromObject(object) {
            if (object instanceof $root.login.VerifyTokenReq)
                return object;
            var message = new $root.login.VerifyTokenReq();
            if (object.token != null)
                message.token = String(object.token);
            return message;
        };

        /**
         * Creates a plain object from a VerifyTokenReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.VerifyTokenReq
         * @static
         * @param {login.VerifyTokenReq} message VerifyTokenReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        VerifyTokenReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.token = "";
            if (message.token != null && message.hasOwnProperty("token"))
                object.token = message.token;
            return object;
        };

        /**
         * Converts this VerifyTokenReq to JSON.
         * @function toJSON
         * @memberof login.VerifyTokenReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        VerifyTokenReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for VerifyTokenReq
         * @function getTypeUrl
         * @memberof login.VerifyTokenReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        VerifyTokenReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.VerifyTokenReq";
        };

        return VerifyTokenReq;
    })();

    login.VerifyTokenRes = (function() {

        /**
         * Properties of a VerifyTokenRes.
         * @memberof login
         * @interface IVerifyTokenRes
         * @property {common.IResult|null} [result] VerifyTokenRes result
         * @property {number|Long|null} [playerId] VerifyTokenRes playerId
         * @property {number|Long|null} [expireAt] VerifyTokenRes expireAt
         */

        /**
         * Constructs a new VerifyTokenRes.
         * @memberof login
         * @classdesc Represents a VerifyTokenRes.
         * @implements IVerifyTokenRes
         * @constructor
         * @param {login.IVerifyTokenRes=} [properties] Properties to set
         */
        function VerifyTokenRes(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * VerifyTokenRes result.
         * @member {common.IResult|null|undefined} result
         * @memberof login.VerifyTokenRes
         * @instance
         */
        VerifyTokenRes.prototype.result = null;

        /**
         * VerifyTokenRes playerId.
         * @member {number|Long} playerId
         * @memberof login.VerifyTokenRes
         * @instance
         */
        VerifyTokenRes.prototype.playerId = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * VerifyTokenRes expireAt.
         * @member {number|Long} expireAt
         * @memberof login.VerifyTokenRes
         * @instance
         */
        VerifyTokenRes.prototype.expireAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new VerifyTokenRes instance using the specified properties.
         * @function create
         * @memberof login.VerifyTokenRes
         * @static
         * @param {login.IVerifyTokenRes=} [properties] Properties to set
         * @returns {login.VerifyTokenRes} VerifyTokenRes instance
         */
        VerifyTokenRes.create = function create(properties) {
            return new VerifyTokenRes(properties);
        };

        /**
         * Encodes the specified VerifyTokenRes message. Does not implicitly {@link login.VerifyTokenRes.verify|verify} messages.
         * @function encode
         * @memberof login.VerifyTokenRes
         * @static
         * @param {login.IVerifyTokenRes} message VerifyTokenRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        VerifyTokenRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.result != null && Object.hasOwnProperty.call(message, "result"))
                $root.common.Result.encode(message.result, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.playerId != null && Object.hasOwnProperty.call(message, "playerId"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint64(message.playerId);
            if (message.expireAt != null && Object.hasOwnProperty.call(message, "expireAt"))
                writer.uint32(/* id 3, wireType 0 =*/24).uint64(message.expireAt);
            return writer;
        };

        /**
         * Encodes the specified VerifyTokenRes message, length delimited. Does not implicitly {@link login.VerifyTokenRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof login.VerifyTokenRes
         * @static
         * @param {login.IVerifyTokenRes} message VerifyTokenRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        VerifyTokenRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a VerifyTokenRes message from the specified reader or buffer.
         * @function decode
         * @memberof login.VerifyTokenRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {login.VerifyTokenRes} VerifyTokenRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        VerifyTokenRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.login.VerifyTokenRes();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.result = $root.common.Result.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.playerId = reader.uint64();
                        break;
                    }
                case 3: {
                        message.expireAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a VerifyTokenRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof login.VerifyTokenRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {login.VerifyTokenRes} VerifyTokenRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        VerifyTokenRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a VerifyTokenRes message.
         * @function verify
         * @memberof login.VerifyTokenRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        VerifyTokenRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.result != null && message.hasOwnProperty("result")) {
                var error = $root.common.Result.verify(message.result);
                if (error)
                    return "result." + error;
            }
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (!$util.isInteger(message.playerId) && !(message.playerId && $util.isInteger(message.playerId.low) && $util.isInteger(message.playerId.high)))
                    return "playerId: integer|Long expected";
            if (message.expireAt != null && message.hasOwnProperty("expireAt"))
                if (!$util.isInteger(message.expireAt) && !(message.expireAt && $util.isInteger(message.expireAt.low) && $util.isInteger(message.expireAt.high)))
                    return "expireAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a VerifyTokenRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof login.VerifyTokenRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {login.VerifyTokenRes} VerifyTokenRes
         */
        VerifyTokenRes.fromObject = function fromObject(object) {
            if (object instanceof $root.login.VerifyTokenRes)
                return object;
            var message = new $root.login.VerifyTokenRes();
            if (object.result != null) {
                if (typeof object.result !== "object")
                    throw TypeError(".login.VerifyTokenRes.result: object expected");
                message.result = $root.common.Result.fromObject(object.result);
            }
            if (object.playerId != null)
                if ($util.Long)
                    (message.playerId = $util.Long.fromValue(object.playerId)).unsigned = true;
                else if (typeof object.playerId === "string")
                    message.playerId = parseInt(object.playerId, 10);
                else if (typeof object.playerId === "number")
                    message.playerId = object.playerId;
                else if (typeof object.playerId === "object")
                    message.playerId = new $util.LongBits(object.playerId.low >>> 0, object.playerId.high >>> 0).toNumber(true);
            if (object.expireAt != null)
                if ($util.Long)
                    (message.expireAt = $util.Long.fromValue(object.expireAt)).unsigned = true;
                else if (typeof object.expireAt === "string")
                    message.expireAt = parseInt(object.expireAt, 10);
                else if (typeof object.expireAt === "number")
                    message.expireAt = object.expireAt;
                else if (typeof object.expireAt === "object")
                    message.expireAt = new $util.LongBits(object.expireAt.low >>> 0, object.expireAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a VerifyTokenRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof login.VerifyTokenRes
         * @static
         * @param {login.VerifyTokenRes} message VerifyTokenRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        VerifyTokenRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.result = null;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.playerId = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.playerId = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.expireAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.expireAt = options.longs === String ? "0" : 0;
            }
            if (message.result != null && message.hasOwnProperty("result"))
                object.result = $root.common.Result.toObject(message.result, options);
            if (message.playerId != null && message.hasOwnProperty("playerId"))
                if (typeof message.playerId === "number")
                    object.playerId = options.longs === String ? String(message.playerId) : message.playerId;
                else
                    object.playerId = options.longs === String ? $util.Long.prototype.toString.call(message.playerId) : options.longs === Number ? new $util.LongBits(message.playerId.low >>> 0, message.playerId.high >>> 0).toNumber(true) : message.playerId;
            if (message.expireAt != null && message.hasOwnProperty("expireAt"))
                if (typeof message.expireAt === "number")
                    object.expireAt = options.longs === String ? String(message.expireAt) : message.expireAt;
                else
                    object.expireAt = options.longs === String ? $util.Long.prototype.toString.call(message.expireAt) : options.longs === Number ? new $util.LongBits(message.expireAt.low >>> 0, message.expireAt.high >>> 0).toNumber(true) : message.expireAt;
            return object;
        };

        /**
         * Converts this VerifyTokenRes to JSON.
         * @function toJSON
         * @memberof login.VerifyTokenRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        VerifyTokenRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for VerifyTokenRes
         * @function getTypeUrl
         * @memberof login.VerifyTokenRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        VerifyTokenRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/login.VerifyTokenRes";
        };

        return VerifyTokenRes;
    })();

    return login;
})();

$root.protocol = (function() {

    /**
     * Namespace protocol.
     * @exports protocol
     * @namespace
     */
    var protocol = {};

    /**
     * ServiceType enum.
     * @name protocol.ServiceType
     * @enum {number}
     * @property {number} SERVICE_UNKNOWN=0 SERVICE_UNKNOWN value
     * @property {number} SERVICE_REGISTRY=1 SERVICE_REGISTRY value
     * @property {number} SERVICE_GATEWAY=2 SERVICE_GATEWAY value
     * @property {number} SERVICE_DBPROXY=3 SERVICE_DBPROXY value
     * @property {number} SERVICE_BIZ=10 SERVICE_BIZ value
     * @property {number} SERVICE_CHAT=11 SERVICE_CHAT value
     * @property {number} SERVICE_LOGIN=12 SERVICE_LOGIN value
     * @property {number} SERVICE_LOGSTATS=13 SERVICE_LOGSTATS value
     * @property {number} SERVICE_REALTIME=20 SERVICE_REALTIME value
     * @property {number} SERVICE_BOT=30 SERVICE_BOT value
     */
    protocol.ServiceType = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "SERVICE_UNKNOWN"] = 0;
        values[valuesById[1] = "SERVICE_REGISTRY"] = 1;
        values[valuesById[2] = "SERVICE_GATEWAY"] = 2;
        values[valuesById[3] = "SERVICE_DBPROXY"] = 3;
        values[valuesById[10] = "SERVICE_BIZ"] = 10;
        values[valuesById[11] = "SERVICE_CHAT"] = 11;
        values[valuesById[12] = "SERVICE_LOGIN"] = 12;
        values[valuesById[13] = "SERVICE_LOGSTATS"] = 13;
        values[valuesById[20] = "SERVICE_REALTIME"] = 20;
        values[valuesById[30] = "SERVICE_BOT"] = 30;
        return values;
    })();

    /**
     * CmdSystem enum.
     * @name protocol.CmdSystem
     * @enum {number}
     * @property {number} CMD_SYS_UNKNOWN=0 CMD_SYS_UNKNOWN value
     * @property {number} CMD_SYS_HEARTBEAT=1 CMD_SYS_HEARTBEAT value
     * @property {number} CMD_SYS_HANDSHAKE=2 CMD_SYS_HANDSHAKE value
     * @property {number} CMD_SYS_ERROR_PACKET=3 CMD_SYS_ERROR_PACKET value
     */
    protocol.CmdSystem = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_SYS_UNKNOWN"] = 0;
        values[valuesById[1] = "CMD_SYS_HEARTBEAT"] = 1;
        values[valuesById[2] = "CMD_SYS_HANDSHAKE"] = 2;
        values[valuesById[3] = "CMD_SYS_ERROR_PACKET"] = 3;
        return values;
    })();

    /**
     * CmdCommon enum.
     * @name protocol.CmdCommon
     * @enum {number}
     * @property {number} CMD_CMN_UNKNOWN=0 CMD_CMN_UNKNOWN value
     * @property {number} CMD_CMN_LOGIN_REQ=4096 CMD_CMN_LOGIN_REQ value
     * @property {number} CMD_CMN_LOGIN_RES=4097 CMD_CMN_LOGIN_RES value
     * @property {number} CMD_CMN_REGISTER_REQ=4098 CMD_CMN_REGISTER_REQ value
     * @property {number} CMD_CMN_REGISTER_RES=4099 CMD_CMN_REGISTER_RES value
     * @property {number} CMD_CMN_GATEWAY_FORWARD=4100 CMD_CMN_GATEWAY_FORWARD value
     */
    protocol.CmdCommon = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_CMN_UNKNOWN"] = 0;
        values[valuesById[4096] = "CMD_CMN_LOGIN_REQ"] = 4096;
        values[valuesById[4097] = "CMD_CMN_LOGIN_RES"] = 4097;
        values[valuesById[4098] = "CMD_CMN_REGISTER_REQ"] = 4098;
        values[valuesById[4099] = "CMD_CMN_REGISTER_RES"] = 4099;
        values[valuesById[4100] = "CMD_CMN_GATEWAY_FORWARD"] = 4100;
        return values;
    })();

    /**
     * CmdGatewayInternal enum.
     * @name protocol.CmdGatewayInternal
     * @enum {number}
     * @property {number} CMD_GW_UNKNOWN=0 CMD_GW_UNKNOWN value
     * @property {number} CMD_GW_ROOM_JOIN=4112 CMD_GW_ROOM_JOIN value
     * @property {number} CMD_GW_ROOM_LEAVE=4113 CMD_GW_ROOM_LEAVE value
     */
    protocol.CmdGatewayInternal = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_GW_UNKNOWN"] = 0;
        values[valuesById[4112] = "CMD_GW_ROOM_JOIN"] = 4112;
        values[valuesById[4113] = "CMD_GW_ROOM_LEAVE"] = 4113;
        return values;
    })();

    /**
     * CmdRegistryInternal enum.
     * @name protocol.CmdRegistryInternal
     * @enum {number}
     * @property {number} CMD_REG_INT_UNKNOWN=0 CMD_REG_INT_UNKNOWN value
     * @property {number} CMD_REG_INT_REGISTER=983041 CMD_REG_INT_REGISTER value
     * @property {number} CMD_REG_INT_HEARTBEAT=983042 CMD_REG_INT_HEARTBEAT value
     * @property {number} CMD_REG_INT_DISCOVER=983043 CMD_REG_INT_DISCOVER value
     * @property {number} CMD_REG_INT_WATCH=983044 CMD_REG_INT_WATCH value
     * @property {number} CMD_REG_INT_NODE_EVENT=983045 CMD_REG_INT_NODE_EVENT value
     * @property {number} CMD_REG_INT_SUBSCRIBE=983046 CMD_REG_INT_SUBSCRIBE value
     */
    protocol.CmdRegistryInternal = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_REG_INT_UNKNOWN"] = 0;
        values[valuesById[983041] = "CMD_REG_INT_REGISTER"] = 983041;
        values[valuesById[983042] = "CMD_REG_INT_HEARTBEAT"] = 983042;
        values[valuesById[983043] = "CMD_REG_INT_DISCOVER"] = 983043;
        values[valuesById[983044] = "CMD_REG_INT_WATCH"] = 983044;
        values[valuesById[983045] = "CMD_REG_INT_NODE_EVENT"] = 983045;
        values[valuesById[983046] = "CMD_REG_INT_SUBSCRIBE"] = 983046;
        return values;
    })();

    /**
     * CmdDBProxyInternal enum.
     * @name protocol.CmdDBProxyInternal
     * @enum {number}
     * @property {number} CMD_DB_INT_UNKNOWN=0 CMD_DB_INT_UNKNOWN value
     * @property {number} CMD_DB_INT_MYSQL_QUERY=917505 CMD_DB_INT_MYSQL_QUERY value
     * @property {number} CMD_DB_INT_MYSQL_QUERY_RES=917506 CMD_DB_INT_MYSQL_QUERY_RES value
     * @property {number} CMD_DB_INT_MYSQL_EXEC=917507 CMD_DB_INT_MYSQL_EXEC value
     * @property {number} CMD_DB_INT_MYSQL_EXEC_RES=917508 CMD_DB_INT_MYSQL_EXEC_RES value
     */
    protocol.CmdDBProxyInternal = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_DB_INT_UNKNOWN"] = 0;
        values[valuesById[917505] = "CMD_DB_INT_MYSQL_QUERY"] = 917505;
        values[valuesById[917506] = "CMD_DB_INT_MYSQL_QUERY_RES"] = 917506;
        values[valuesById[917507] = "CMD_DB_INT_MYSQL_EXEC"] = 917507;
        values[valuesById[917508] = "CMD_DB_INT_MYSQL_EXEC_RES"] = 917508;
        return values;
    })();

    /**
     * CmdBiz enum.
     * @name protocol.CmdBiz
     * @enum {number}
     * @property {number} CMD_BIZ_UNKNOWN=0 CMD_BIZ_UNKNOWN value
     * @property {number} CMD_BIZ_GET_PLAYER_REQ=65536 CMD_BIZ_GET_PLAYER_REQ value
     * @property {number} CMD_BIZ_GET_PLAYER_RES=65537 CMD_BIZ_GET_PLAYER_RES value
     * @property {number} CMD_BIZ_GET_BAG_REQ=65538 CMD_BIZ_GET_BAG_REQ value
     * @property {number} CMD_BIZ_GET_BAG_RES=65539 CMD_BIZ_GET_BAG_RES value
     * @property {number} CMD_BIZ_PING=65540 CMD_BIZ_PING value
     * @property {number} CMD_BIZ_PONG=65541 CMD_BIZ_PONG value
     * @property {number} CMD_BIZ_UPDATE_PLAYER_REQ=65542 CMD_BIZ_UPDATE_PLAYER_REQ value
     * @property {number} CMD_BIZ_UPDATE_PLAYER_RES=65543 CMD_BIZ_UPDATE_PLAYER_RES value
     * @property {number} CMD_BIZ_GET_PLAYER_ROOMS_REQ=65544 CMD_BIZ_GET_PLAYER_ROOMS_REQ value
     * @property {number} CMD_BIZ_GET_PLAYER_ROOMS_RES=65545 CMD_BIZ_GET_PLAYER_ROOMS_RES value
     */
    protocol.CmdBiz = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_BIZ_UNKNOWN"] = 0;
        values[valuesById[65536] = "CMD_BIZ_GET_PLAYER_REQ"] = 65536;
        values[valuesById[65537] = "CMD_BIZ_GET_PLAYER_RES"] = 65537;
        values[valuesById[65538] = "CMD_BIZ_GET_BAG_REQ"] = 65538;
        values[valuesById[65539] = "CMD_BIZ_GET_BAG_RES"] = 65539;
        values[valuesById[65540] = "CMD_BIZ_PING"] = 65540;
        values[valuesById[65541] = "CMD_BIZ_PONG"] = 65541;
        values[valuesById[65542] = "CMD_BIZ_UPDATE_PLAYER_REQ"] = 65542;
        values[valuesById[65543] = "CMD_BIZ_UPDATE_PLAYER_RES"] = 65543;
        values[valuesById[65544] = "CMD_BIZ_GET_PLAYER_ROOMS_REQ"] = 65544;
        values[valuesById[65545] = "CMD_BIZ_GET_PLAYER_ROOMS_RES"] = 65545;
        return values;
    })();

    /**
     * CmdChat enum.
     * @name protocol.CmdChat
     * @enum {number}
     * @property {number} CMD_CHAT_UNKNOWN=0 CMD_CHAT_UNKNOWN value
     * @property {number} CMD_CHAT_CREATE_ROOM_REQ=196608 CMD_CHAT_CREATE_ROOM_REQ value
     * @property {number} CMD_CHAT_CREATE_ROOM_RES=196609 CMD_CHAT_CREATE_ROOM_RES value
     * @property {number} CMD_CHAT_JOIN_ROOM_REQ=196610 CMD_CHAT_JOIN_ROOM_REQ value
     * @property {number} CMD_CHAT_JOIN_ROOM_RES=196611 CMD_CHAT_JOIN_ROOM_RES value
     * @property {number} CMD_CHAT_LEAVE_ROOM_REQ=196612 CMD_CHAT_LEAVE_ROOM_REQ value
     * @property {number} CMD_CHAT_LEAVE_ROOM_RES=196613 CMD_CHAT_LEAVE_ROOM_RES value
     * @property {number} CMD_CHAT_SEND_MSG_REQ=196614 CMD_CHAT_SEND_MSG_REQ value
     * @property {number} CMD_CHAT_SEND_MSG_RES=196615 CMD_CHAT_SEND_MSG_RES value
     * @property {number} CMD_CHAT_MSG_NOTIFY=196616 CMD_CHAT_MSG_NOTIFY value
     * @property {number} CMD_CHAT_GET_HISTORY_REQ=196617 CMD_CHAT_GET_HISTORY_REQ value
     * @property {number} CMD_CHAT_GET_HISTORY_RES=196618 CMD_CHAT_GET_HISTORY_RES value
     * @property {number} CMD_CHAT_CLOSE_ROOM_REQ=196619 CMD_CHAT_CLOSE_ROOM_REQ value
     * @property {number} CMD_CHAT_CLOSE_ROOM_RES=196620 CMD_CHAT_CLOSE_ROOM_RES value
     * @property {number} CMD_CHAT_LIST_ROOM_REQ=196621 CMD_CHAT_LIST_ROOM_REQ value
     * @property {number} CMD_CHAT_LIST_ROOM_RES=196622 CMD_CHAT_LIST_ROOM_RES value
     */
    protocol.CmdChat = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_CHAT_UNKNOWN"] = 0;
        values[valuesById[196608] = "CMD_CHAT_CREATE_ROOM_REQ"] = 196608;
        values[valuesById[196609] = "CMD_CHAT_CREATE_ROOM_RES"] = 196609;
        values[valuesById[196610] = "CMD_CHAT_JOIN_ROOM_REQ"] = 196610;
        values[valuesById[196611] = "CMD_CHAT_JOIN_ROOM_RES"] = 196611;
        values[valuesById[196612] = "CMD_CHAT_LEAVE_ROOM_REQ"] = 196612;
        values[valuesById[196613] = "CMD_CHAT_LEAVE_ROOM_RES"] = 196613;
        values[valuesById[196614] = "CMD_CHAT_SEND_MSG_REQ"] = 196614;
        values[valuesById[196615] = "CMD_CHAT_SEND_MSG_RES"] = 196615;
        values[valuesById[196616] = "CMD_CHAT_MSG_NOTIFY"] = 196616;
        values[valuesById[196617] = "CMD_CHAT_GET_HISTORY_REQ"] = 196617;
        values[valuesById[196618] = "CMD_CHAT_GET_HISTORY_RES"] = 196618;
        values[valuesById[196619] = "CMD_CHAT_CLOSE_ROOM_REQ"] = 196619;
        values[valuesById[196620] = "CMD_CHAT_CLOSE_ROOM_RES"] = 196620;
        values[valuesById[196621] = "CMD_CHAT_LIST_ROOM_REQ"] = 196621;
        values[valuesById[196622] = "CMD_CHAT_LIST_ROOM_RES"] = 196622;
        return values;
    })();

    /**
     * CmdLogStats enum.
     * @name protocol.CmdLogStats
     * @enum {number}
     * @property {number} CMD_LOGSTATS_UNKNOWN=0 CMD_LOGSTATS_UNKNOWN value
     * @property {number} CMD_LOGSTATS_BASE=262144 CMD_LOGSTATS_BASE value
     */
    protocol.CmdLogStats = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_LOGSTATS_UNKNOWN"] = 0;
        values[valuesById[262144] = "CMD_LOGSTATS_BASE"] = 262144;
        return values;
    })();

    /**
     * CmdRealtime enum.
     * @name protocol.CmdRealtime
     * @enum {number}
     * @property {number} CMD_RT_UNKNOWN=0 CMD_RT_UNKNOWN value
     * @property {number} CMD_RT_ROOM_ENTER_REQ=131072 CMD_RT_ROOM_ENTER_REQ value
     * @property {number} CMD_RT_ROOM_ENTER_RES=131073 CMD_RT_ROOM_ENTER_RES value
     * @property {number} CMD_RT_ROOM_LEAVE_REQ=131074 CMD_RT_ROOM_LEAVE_REQ value
     * @property {number} CMD_RT_ROOM_LEAVE_RES=131075 CMD_RT_ROOM_LEAVE_RES value
     * @property {number} CMD_RT_FRAME_SYNC=131076 CMD_RT_FRAME_SYNC value
     * @property {number} CMD_RT_STATE_SYNC=131077 CMD_RT_STATE_SYNC value
     */
    protocol.CmdRealtime = (function() {
        var valuesById = {}, values = Object.create(valuesById);
        values[valuesById[0] = "CMD_RT_UNKNOWN"] = 0;
        values[valuesById[131072] = "CMD_RT_ROOM_ENTER_REQ"] = 131072;
        values[valuesById[131073] = "CMD_RT_ROOM_ENTER_RES"] = 131073;
        values[valuesById[131074] = "CMD_RT_ROOM_LEAVE_REQ"] = 131074;
        values[valuesById[131075] = "CMD_RT_ROOM_LEAVE_RES"] = 131075;
        values[valuesById[131076] = "CMD_RT_FRAME_SYNC"] = 131076;
        values[valuesById[131077] = "CMD_RT_STATE_SYNC"] = 131077;
        return values;
    })();

    protocol.CmdRange = (function() {

        /**
         * Properties of a CmdRange.
         * @memberof protocol
         * @interface ICmdRange
         * @property {number|null} [start] CmdRange start
         * @property {number|null} [end] CmdRange end
         */

        /**
         * Constructs a new CmdRange.
         * @memberof protocol
         * @classdesc Represents a CmdRange.
         * @implements ICmdRange
         * @constructor
         * @param {protocol.ICmdRange=} [properties] Properties to set
         */
        function CmdRange(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * CmdRange start.
         * @member {number} start
         * @memberof protocol.CmdRange
         * @instance
         */
        CmdRange.prototype.start = 0;

        /**
         * CmdRange end.
         * @member {number} end
         * @memberof protocol.CmdRange
         * @instance
         */
        CmdRange.prototype.end = 0;

        /**
         * Creates a new CmdRange instance using the specified properties.
         * @function create
         * @memberof protocol.CmdRange
         * @static
         * @param {protocol.ICmdRange=} [properties] Properties to set
         * @returns {protocol.CmdRange} CmdRange instance
         */
        CmdRange.create = function create(properties) {
            return new CmdRange(properties);
        };

        /**
         * Encodes the specified CmdRange message. Does not implicitly {@link protocol.CmdRange.verify|verify} messages.
         * @function encode
         * @memberof protocol.CmdRange
         * @static
         * @param {protocol.ICmdRange} message CmdRange message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        CmdRange.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.start != null && Object.hasOwnProperty.call(message, "start"))
                writer.uint32(/* id 1, wireType 0 =*/8).uint32(message.start);
            if (message.end != null && Object.hasOwnProperty.call(message, "end"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.end);
            return writer;
        };

        /**
         * Encodes the specified CmdRange message, length delimited. Does not implicitly {@link protocol.CmdRange.verify|verify} messages.
         * @function encodeDelimited
         * @memberof protocol.CmdRange
         * @static
         * @param {protocol.ICmdRange} message CmdRange message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        CmdRange.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a CmdRange message from the specified reader or buffer.
         * @function decode
         * @memberof protocol.CmdRange
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {protocol.CmdRange} CmdRange
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        CmdRange.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.protocol.CmdRange();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.start = reader.uint32();
                        break;
                    }
                case 2: {
                        message.end = reader.uint32();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a CmdRange message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof protocol.CmdRange
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {protocol.CmdRange} CmdRange
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        CmdRange.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a CmdRange message.
         * @function verify
         * @memberof protocol.CmdRange
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        CmdRange.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.start != null && message.hasOwnProperty("start"))
                if (!$util.isInteger(message.start))
                    return "start: integer expected";
            if (message.end != null && message.hasOwnProperty("end"))
                if (!$util.isInteger(message.end))
                    return "end: integer expected";
            return null;
        };

        /**
         * Creates a CmdRange message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof protocol.CmdRange
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {protocol.CmdRange} CmdRange
         */
        CmdRange.fromObject = function fromObject(object) {
            if (object instanceof $root.protocol.CmdRange)
                return object;
            var message = new $root.protocol.CmdRange();
            if (object.start != null)
                message.start = object.start >>> 0;
            if (object.end != null)
                message.end = object.end >>> 0;
            return message;
        };

        /**
         * Creates a plain object from a CmdRange message. Also converts values to other types if specified.
         * @function toObject
         * @memberof protocol.CmdRange
         * @static
         * @param {protocol.CmdRange} message CmdRange
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        CmdRange.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.start = 0;
                object.end = 0;
            }
            if (message.start != null && message.hasOwnProperty("start"))
                object.start = message.start;
            if (message.end != null && message.hasOwnProperty("end"))
                object.end = message.end;
            return object;
        };

        /**
         * Converts this CmdRange to JSON.
         * @function toJSON
         * @memberof protocol.CmdRange
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        CmdRange.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for CmdRange
         * @function getTypeUrl
         * @memberof protocol.CmdRange
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        CmdRange.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/protocol.CmdRange";
        };

        return CmdRange;
    })();

    protocol.ServiceCmdRanges = (function() {

        /**
         * Properties of a ServiceCmdRanges.
         * @memberof protocol
         * @interface IServiceCmdRanges
         * @property {protocol.ICmdRange|null} [chat] ServiceCmdRanges chat
         * @property {protocol.ICmdRange|null} [biz] ServiceCmdRanges biz
         * @property {protocol.ICmdRange|null} [login] ServiceCmdRanges login
         * @property {protocol.ICmdRange|null} [realtime] ServiceCmdRanges realtime
         */

        /**
         * Constructs a new ServiceCmdRanges.
         * @memberof protocol
         * @classdesc Represents a ServiceCmdRanges.
         * @implements IServiceCmdRanges
         * @constructor
         * @param {protocol.IServiceCmdRanges=} [properties] Properties to set
         */
        function ServiceCmdRanges(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ServiceCmdRanges chat.
         * @member {protocol.ICmdRange|null|undefined} chat
         * @memberof protocol.ServiceCmdRanges
         * @instance
         */
        ServiceCmdRanges.prototype.chat = null;

        /**
         * ServiceCmdRanges biz.
         * @member {protocol.ICmdRange|null|undefined} biz
         * @memberof protocol.ServiceCmdRanges
         * @instance
         */
        ServiceCmdRanges.prototype.biz = null;

        /**
         * ServiceCmdRanges login.
         * @member {protocol.ICmdRange|null|undefined} login
         * @memberof protocol.ServiceCmdRanges
         * @instance
         */
        ServiceCmdRanges.prototype.login = null;

        /**
         * ServiceCmdRanges realtime.
         * @member {protocol.ICmdRange|null|undefined} realtime
         * @memberof protocol.ServiceCmdRanges
         * @instance
         */
        ServiceCmdRanges.prototype.realtime = null;

        /**
         * Creates a new ServiceCmdRanges instance using the specified properties.
         * @function create
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {protocol.IServiceCmdRanges=} [properties] Properties to set
         * @returns {protocol.ServiceCmdRanges} ServiceCmdRanges instance
         */
        ServiceCmdRanges.create = function create(properties) {
            return new ServiceCmdRanges(properties);
        };

        /**
         * Encodes the specified ServiceCmdRanges message. Does not implicitly {@link protocol.ServiceCmdRanges.verify|verify} messages.
         * @function encode
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {protocol.IServiceCmdRanges} message ServiceCmdRanges message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ServiceCmdRanges.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.chat != null && Object.hasOwnProperty.call(message, "chat"))
                $root.protocol.CmdRange.encode(message.chat, writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            if (message.biz != null && Object.hasOwnProperty.call(message, "biz"))
                $root.protocol.CmdRange.encode(message.biz, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            if (message.login != null && Object.hasOwnProperty.call(message, "login"))
                $root.protocol.CmdRange.encode(message.login, writer.uint32(/* id 3, wireType 2 =*/26).fork()).ldelim();
            if (message.realtime != null && Object.hasOwnProperty.call(message, "realtime"))
                $root.protocol.CmdRange.encode(message.realtime, writer.uint32(/* id 4, wireType 2 =*/34).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified ServiceCmdRanges message, length delimited. Does not implicitly {@link protocol.ServiceCmdRanges.verify|verify} messages.
         * @function encodeDelimited
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {protocol.IServiceCmdRanges} message ServiceCmdRanges message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ServiceCmdRanges.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ServiceCmdRanges message from the specified reader or buffer.
         * @function decode
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {protocol.ServiceCmdRanges} ServiceCmdRanges
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ServiceCmdRanges.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.protocol.ServiceCmdRanges();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.chat = $root.protocol.CmdRange.decode(reader, reader.uint32());
                        break;
                    }
                case 2: {
                        message.biz = $root.protocol.CmdRange.decode(reader, reader.uint32());
                        break;
                    }
                case 3: {
                        message.login = $root.protocol.CmdRange.decode(reader, reader.uint32());
                        break;
                    }
                case 4: {
                        message.realtime = $root.protocol.CmdRange.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ServiceCmdRanges message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {protocol.ServiceCmdRanges} ServiceCmdRanges
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ServiceCmdRanges.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ServiceCmdRanges message.
         * @function verify
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ServiceCmdRanges.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.chat != null && message.hasOwnProperty("chat")) {
                var error = $root.protocol.CmdRange.verify(message.chat);
                if (error)
                    return "chat." + error;
            }
            if (message.biz != null && message.hasOwnProperty("biz")) {
                var error = $root.protocol.CmdRange.verify(message.biz);
                if (error)
                    return "biz." + error;
            }
            if (message.login != null && message.hasOwnProperty("login")) {
                var error = $root.protocol.CmdRange.verify(message.login);
                if (error)
                    return "login." + error;
            }
            if (message.realtime != null && message.hasOwnProperty("realtime")) {
                var error = $root.protocol.CmdRange.verify(message.realtime);
                if (error)
                    return "realtime." + error;
            }
            return null;
        };

        /**
         * Creates a ServiceCmdRanges message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {protocol.ServiceCmdRanges} ServiceCmdRanges
         */
        ServiceCmdRanges.fromObject = function fromObject(object) {
            if (object instanceof $root.protocol.ServiceCmdRanges)
                return object;
            var message = new $root.protocol.ServiceCmdRanges();
            if (object.chat != null) {
                if (typeof object.chat !== "object")
                    throw TypeError(".protocol.ServiceCmdRanges.chat: object expected");
                message.chat = $root.protocol.CmdRange.fromObject(object.chat);
            }
            if (object.biz != null) {
                if (typeof object.biz !== "object")
                    throw TypeError(".protocol.ServiceCmdRanges.biz: object expected");
                message.biz = $root.protocol.CmdRange.fromObject(object.biz);
            }
            if (object.login != null) {
                if (typeof object.login !== "object")
                    throw TypeError(".protocol.ServiceCmdRanges.login: object expected");
                message.login = $root.protocol.CmdRange.fromObject(object.login);
            }
            if (object.realtime != null) {
                if (typeof object.realtime !== "object")
                    throw TypeError(".protocol.ServiceCmdRanges.realtime: object expected");
                message.realtime = $root.protocol.CmdRange.fromObject(object.realtime);
            }
            return message;
        };

        /**
         * Creates a plain object from a ServiceCmdRanges message. Also converts values to other types if specified.
         * @function toObject
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {protocol.ServiceCmdRanges} message ServiceCmdRanges
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ServiceCmdRanges.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.chat = null;
                object.biz = null;
                object.login = null;
                object.realtime = null;
            }
            if (message.chat != null && message.hasOwnProperty("chat"))
                object.chat = $root.protocol.CmdRange.toObject(message.chat, options);
            if (message.biz != null && message.hasOwnProperty("biz"))
                object.biz = $root.protocol.CmdRange.toObject(message.biz, options);
            if (message.login != null && message.hasOwnProperty("login"))
                object.login = $root.protocol.CmdRange.toObject(message.login, options);
            if (message.realtime != null && message.hasOwnProperty("realtime"))
                object.realtime = $root.protocol.CmdRange.toObject(message.realtime, options);
            return object;
        };

        /**
         * Converts this ServiceCmdRanges to JSON.
         * @function toJSON
         * @memberof protocol.ServiceCmdRanges
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ServiceCmdRanges.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ServiceCmdRanges
         * @function getTypeUrl
         * @memberof protocol.ServiceCmdRanges
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ServiceCmdRanges.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/protocol.ServiceCmdRanges";
        };

        return ServiceCmdRanges;
    })();

    return protocol;
})();

$root.registry = (function() {

    /**
     * Namespace registry.
     * @exports registry
     * @namespace
     */
    var registry = {};

    registry.NodeInfo = (function() {

        /**
         * Properties of a NodeInfo.
         * @memberof registry
         * @interface INodeInfo
         * @property {string|null} [serviceType] NodeInfo serviceType
         * @property {string|null} [nodeId] NodeInfo nodeId
         * @property {string|null} [host] NodeInfo host
         * @property {number|null} [port] NodeInfo port
         * @property {Object.<string,string>|null} [metadata] NodeInfo metadata
         * @property {number|Long|null} [loadScore] NodeInfo loadScore
         * @property {number|Long|null} [registerAt] NodeInfo registerAt
         */

        /**
         * Constructs a new NodeInfo.
         * @memberof registry
         * @classdesc Represents a NodeInfo.
         * @implements INodeInfo
         * @constructor
         * @param {registry.INodeInfo=} [properties] Properties to set
         */
        function NodeInfo(properties) {
            this.metadata = {};
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * NodeInfo serviceType.
         * @member {string} serviceType
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.serviceType = "";

        /**
         * NodeInfo nodeId.
         * @member {string} nodeId
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.nodeId = "";

        /**
         * NodeInfo host.
         * @member {string} host
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.host = "";

        /**
         * NodeInfo port.
         * @member {number} port
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.port = 0;

        /**
         * NodeInfo metadata.
         * @member {Object.<string,string>} metadata
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.metadata = $util.emptyObject;

        /**
         * NodeInfo loadScore.
         * @member {number|Long} loadScore
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.loadScore = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * NodeInfo registerAt.
         * @member {number|Long} registerAt
         * @memberof registry.NodeInfo
         * @instance
         */
        NodeInfo.prototype.registerAt = $util.Long ? $util.Long.fromBits(0,0,true) : 0;

        /**
         * Creates a new NodeInfo instance using the specified properties.
         * @function create
         * @memberof registry.NodeInfo
         * @static
         * @param {registry.INodeInfo=} [properties] Properties to set
         * @returns {registry.NodeInfo} NodeInfo instance
         */
        NodeInfo.create = function create(properties) {
            return new NodeInfo(properties);
        };

        /**
         * Encodes the specified NodeInfo message. Does not implicitly {@link registry.NodeInfo.verify|verify} messages.
         * @function encode
         * @memberof registry.NodeInfo
         * @static
         * @param {registry.INodeInfo} message NodeInfo message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeInfo.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.serviceType != null && Object.hasOwnProperty.call(message, "serviceType"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.serviceType);
            if (message.nodeId != null && Object.hasOwnProperty.call(message, "nodeId"))
                writer.uint32(/* id 2, wireType 2 =*/18).string(message.nodeId);
            if (message.host != null && Object.hasOwnProperty.call(message, "host"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.host);
            if (message.port != null && Object.hasOwnProperty.call(message, "port"))
                writer.uint32(/* id 4, wireType 0 =*/32).uint32(message.port);
            if (message.metadata != null && Object.hasOwnProperty.call(message, "metadata"))
                for (var keys = Object.keys(message.metadata), i = 0; i < keys.length; ++i)
                    writer.uint32(/* id 5, wireType 2 =*/42).fork().uint32(/* id 1, wireType 2 =*/10).string(keys[i]).uint32(/* id 2, wireType 2 =*/18).string(message.metadata[keys[i]]).ldelim();
            if (message.loadScore != null && Object.hasOwnProperty.call(message, "loadScore"))
                writer.uint32(/* id 6, wireType 0 =*/48).uint64(message.loadScore);
            if (message.registerAt != null && Object.hasOwnProperty.call(message, "registerAt"))
                writer.uint32(/* id 7, wireType 0 =*/56).uint64(message.registerAt);
            return writer;
        };

        /**
         * Encodes the specified NodeInfo message, length delimited. Does not implicitly {@link registry.NodeInfo.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.NodeInfo
         * @static
         * @param {registry.INodeInfo} message NodeInfo message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeInfo.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a NodeInfo message from the specified reader or buffer.
         * @function decode
         * @memberof registry.NodeInfo
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.NodeInfo} NodeInfo
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeInfo.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.NodeInfo(), key, value;
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.serviceType = reader.string();
                        break;
                    }
                case 2: {
                        message.nodeId = reader.string();
                        break;
                    }
                case 3: {
                        message.host = reader.string();
                        break;
                    }
                case 4: {
                        message.port = reader.uint32();
                        break;
                    }
                case 5: {
                        if (message.metadata === $util.emptyObject)
                            message.metadata = {};
                        var end2 = reader.uint32() + reader.pos;
                        key = "";
                        value = "";
                        while (reader.pos < end2) {
                            var tag2 = reader.uint32();
                            switch (tag2 >>> 3) {
                            case 1:
                                key = reader.string();
                                break;
                            case 2:
                                value = reader.string();
                                break;
                            default:
                                reader.skipType(tag2 & 7);
                                break;
                            }
                        }
                        message.metadata[key] = value;
                        break;
                    }
                case 6: {
                        message.loadScore = reader.uint64();
                        break;
                    }
                case 7: {
                        message.registerAt = reader.uint64();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a NodeInfo message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.NodeInfo
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.NodeInfo} NodeInfo
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeInfo.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a NodeInfo message.
         * @function verify
         * @memberof registry.NodeInfo
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        NodeInfo.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.serviceType != null && message.hasOwnProperty("serviceType"))
                if (!$util.isString(message.serviceType))
                    return "serviceType: string expected";
            if (message.nodeId != null && message.hasOwnProperty("nodeId"))
                if (!$util.isString(message.nodeId))
                    return "nodeId: string expected";
            if (message.host != null && message.hasOwnProperty("host"))
                if (!$util.isString(message.host))
                    return "host: string expected";
            if (message.port != null && message.hasOwnProperty("port"))
                if (!$util.isInteger(message.port))
                    return "port: integer expected";
            if (message.metadata != null && message.hasOwnProperty("metadata")) {
                if (!$util.isObject(message.metadata))
                    return "metadata: object expected";
                var key = Object.keys(message.metadata);
                for (var i = 0; i < key.length; ++i)
                    if (!$util.isString(message.metadata[key[i]]))
                        return "metadata: string{k:string} expected";
            }
            if (message.loadScore != null && message.hasOwnProperty("loadScore"))
                if (!$util.isInteger(message.loadScore) && !(message.loadScore && $util.isInteger(message.loadScore.low) && $util.isInteger(message.loadScore.high)))
                    return "loadScore: integer|Long expected";
            if (message.registerAt != null && message.hasOwnProperty("registerAt"))
                if (!$util.isInteger(message.registerAt) && !(message.registerAt && $util.isInteger(message.registerAt.low) && $util.isInteger(message.registerAt.high)))
                    return "registerAt: integer|Long expected";
            return null;
        };

        /**
         * Creates a NodeInfo message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.NodeInfo
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.NodeInfo} NodeInfo
         */
        NodeInfo.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.NodeInfo)
                return object;
            var message = new $root.registry.NodeInfo();
            if (object.serviceType != null)
                message.serviceType = String(object.serviceType);
            if (object.nodeId != null)
                message.nodeId = String(object.nodeId);
            if (object.host != null)
                message.host = String(object.host);
            if (object.port != null)
                message.port = object.port >>> 0;
            if (object.metadata) {
                if (typeof object.metadata !== "object")
                    throw TypeError(".registry.NodeInfo.metadata: object expected");
                message.metadata = {};
                for (var keys = Object.keys(object.metadata), i = 0; i < keys.length; ++i)
                    message.metadata[keys[i]] = String(object.metadata[keys[i]]);
            }
            if (object.loadScore != null)
                if ($util.Long)
                    (message.loadScore = $util.Long.fromValue(object.loadScore)).unsigned = true;
                else if (typeof object.loadScore === "string")
                    message.loadScore = parseInt(object.loadScore, 10);
                else if (typeof object.loadScore === "number")
                    message.loadScore = object.loadScore;
                else if (typeof object.loadScore === "object")
                    message.loadScore = new $util.LongBits(object.loadScore.low >>> 0, object.loadScore.high >>> 0).toNumber(true);
            if (object.registerAt != null)
                if ($util.Long)
                    (message.registerAt = $util.Long.fromValue(object.registerAt)).unsigned = true;
                else if (typeof object.registerAt === "string")
                    message.registerAt = parseInt(object.registerAt, 10);
                else if (typeof object.registerAt === "number")
                    message.registerAt = object.registerAt;
                else if (typeof object.registerAt === "object")
                    message.registerAt = new $util.LongBits(object.registerAt.low >>> 0, object.registerAt.high >>> 0).toNumber(true);
            return message;
        };

        /**
         * Creates a plain object from a NodeInfo message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.NodeInfo
         * @static
         * @param {registry.NodeInfo} message NodeInfo
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        NodeInfo.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.objects || options.defaults)
                object.metadata = {};
            if (options.defaults) {
                object.serviceType = "";
                object.nodeId = "";
                object.host = "";
                object.port = 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.loadScore = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.loadScore = options.longs === String ? "0" : 0;
                if ($util.Long) {
                    var long = new $util.Long(0, 0, true);
                    object.registerAt = options.longs === String ? long.toString() : options.longs === Number ? long.toNumber() : long;
                } else
                    object.registerAt = options.longs === String ? "0" : 0;
            }
            if (message.serviceType != null && message.hasOwnProperty("serviceType"))
                object.serviceType = message.serviceType;
            if (message.nodeId != null && message.hasOwnProperty("nodeId"))
                object.nodeId = message.nodeId;
            if (message.host != null && message.hasOwnProperty("host"))
                object.host = message.host;
            if (message.port != null && message.hasOwnProperty("port"))
                object.port = message.port;
            var keys2;
            if (message.metadata && (keys2 = Object.keys(message.metadata)).length) {
                object.metadata = {};
                for (var j = 0; j < keys2.length; ++j)
                    object.metadata[keys2[j]] = message.metadata[keys2[j]];
            }
            if (message.loadScore != null && message.hasOwnProperty("loadScore"))
                if (typeof message.loadScore === "number")
                    object.loadScore = options.longs === String ? String(message.loadScore) : message.loadScore;
                else
                    object.loadScore = options.longs === String ? $util.Long.prototype.toString.call(message.loadScore) : options.longs === Number ? new $util.LongBits(message.loadScore.low >>> 0, message.loadScore.high >>> 0).toNumber(true) : message.loadScore;
            if (message.registerAt != null && message.hasOwnProperty("registerAt"))
                if (typeof message.registerAt === "number")
                    object.registerAt = options.longs === String ? String(message.registerAt) : message.registerAt;
                else
                    object.registerAt = options.longs === String ? $util.Long.prototype.toString.call(message.registerAt) : options.longs === Number ? new $util.LongBits(message.registerAt.low >>> 0, message.registerAt.high >>> 0).toNumber(true) : message.registerAt;
            return object;
        };

        /**
         * Converts this NodeInfo to JSON.
         * @function toJSON
         * @memberof registry.NodeInfo
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        NodeInfo.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for NodeInfo
         * @function getTypeUrl
         * @memberof registry.NodeInfo
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        NodeInfo.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.NodeInfo";
        };

        return NodeInfo;
    })();

    registry.NodeList = (function() {

        /**
         * Properties of a NodeList.
         * @memberof registry
         * @interface INodeList
         * @property {Array.<registry.INodeInfo>|null} [nodes] NodeList nodes
         */

        /**
         * Constructs a new NodeList.
         * @memberof registry
         * @classdesc Represents a NodeList.
         * @implements INodeList
         * @constructor
         * @param {registry.INodeList=} [properties] Properties to set
         */
        function NodeList(properties) {
            this.nodes = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * NodeList nodes.
         * @member {Array.<registry.INodeInfo>} nodes
         * @memberof registry.NodeList
         * @instance
         */
        NodeList.prototype.nodes = $util.emptyArray;

        /**
         * Creates a new NodeList instance using the specified properties.
         * @function create
         * @memberof registry.NodeList
         * @static
         * @param {registry.INodeList=} [properties] Properties to set
         * @returns {registry.NodeList} NodeList instance
         */
        NodeList.create = function create(properties) {
            return new NodeList(properties);
        };

        /**
         * Encodes the specified NodeList message. Does not implicitly {@link registry.NodeList.verify|verify} messages.
         * @function encode
         * @memberof registry.NodeList
         * @static
         * @param {registry.INodeList} message NodeList message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeList.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.nodes != null && message.nodes.length)
                for (var i = 0; i < message.nodes.length; ++i)
                    $root.registry.NodeInfo.encode(message.nodes[i], writer.uint32(/* id 1, wireType 2 =*/10).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified NodeList message, length delimited. Does not implicitly {@link registry.NodeList.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.NodeList
         * @static
         * @param {registry.INodeList} message NodeList message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeList.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a NodeList message from the specified reader or buffer.
         * @function decode
         * @memberof registry.NodeList
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.NodeList} NodeList
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeList.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.NodeList();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        if (!(message.nodes && message.nodes.length))
                            message.nodes = [];
                        message.nodes.push($root.registry.NodeInfo.decode(reader, reader.uint32()));
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a NodeList message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.NodeList
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.NodeList} NodeList
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeList.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a NodeList message.
         * @function verify
         * @memberof registry.NodeList
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        NodeList.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.nodes != null && message.hasOwnProperty("nodes")) {
                if (!Array.isArray(message.nodes))
                    return "nodes: array expected";
                for (var i = 0; i < message.nodes.length; ++i) {
                    var error = $root.registry.NodeInfo.verify(message.nodes[i]);
                    if (error)
                        return "nodes." + error;
                }
            }
            return null;
        };

        /**
         * Creates a NodeList message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.NodeList
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.NodeList} NodeList
         */
        NodeList.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.NodeList)
                return object;
            var message = new $root.registry.NodeList();
            if (object.nodes) {
                if (!Array.isArray(object.nodes))
                    throw TypeError(".registry.NodeList.nodes: array expected");
                message.nodes = [];
                for (var i = 0; i < object.nodes.length; ++i) {
                    if (typeof object.nodes[i] !== "object")
                        throw TypeError(".registry.NodeList.nodes: object expected");
                    message.nodes[i] = $root.registry.NodeInfo.fromObject(object.nodes[i]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a NodeList message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.NodeList
         * @static
         * @param {registry.NodeList} message NodeList
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        NodeList.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.nodes = [];
            if (message.nodes && message.nodes.length) {
                object.nodes = [];
                for (var j = 0; j < message.nodes.length; ++j)
                    object.nodes[j] = $root.registry.NodeInfo.toObject(message.nodes[j], options);
            }
            return object;
        };

        /**
         * Converts this NodeList to JSON.
         * @function toJSON
         * @memberof registry.NodeList
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        NodeList.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for NodeList
         * @function getTypeUrl
         * @memberof registry.NodeList
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        NodeList.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.NodeList";
        };

        return NodeList;
    })();

    registry.NodeEvent = (function() {

        /**
         * Properties of a NodeEvent.
         * @memberof registry
         * @interface INodeEvent
         * @property {registry.NodeEvent.Type|null} [type] NodeEvent type
         * @property {registry.INodeInfo|null} [node] NodeEvent node
         */

        /**
         * Constructs a new NodeEvent.
         * @memberof registry
         * @classdesc Represents a NodeEvent.
         * @implements INodeEvent
         * @constructor
         * @param {registry.INodeEvent=} [properties] Properties to set
         */
        function NodeEvent(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * NodeEvent type.
         * @member {registry.NodeEvent.Type} type
         * @memberof registry.NodeEvent
         * @instance
         */
        NodeEvent.prototype.type = 0;

        /**
         * NodeEvent node.
         * @member {registry.INodeInfo|null|undefined} node
         * @memberof registry.NodeEvent
         * @instance
         */
        NodeEvent.prototype.node = null;

        /**
         * Creates a new NodeEvent instance using the specified properties.
         * @function create
         * @memberof registry.NodeEvent
         * @static
         * @param {registry.INodeEvent=} [properties] Properties to set
         * @returns {registry.NodeEvent} NodeEvent instance
         */
        NodeEvent.create = function create(properties) {
            return new NodeEvent(properties);
        };

        /**
         * Encodes the specified NodeEvent message. Does not implicitly {@link registry.NodeEvent.verify|verify} messages.
         * @function encode
         * @memberof registry.NodeEvent
         * @static
         * @param {registry.INodeEvent} message NodeEvent message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeEvent.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.type != null && Object.hasOwnProperty.call(message, "type"))
                writer.uint32(/* id 1, wireType 0 =*/8).int32(message.type);
            if (message.node != null && Object.hasOwnProperty.call(message, "node"))
                $root.registry.NodeInfo.encode(message.node, writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim();
            return writer;
        };

        /**
         * Encodes the specified NodeEvent message, length delimited. Does not implicitly {@link registry.NodeEvent.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.NodeEvent
         * @static
         * @param {registry.INodeEvent} message NodeEvent message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeEvent.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a NodeEvent message from the specified reader or buffer.
         * @function decode
         * @memberof registry.NodeEvent
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.NodeEvent} NodeEvent
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeEvent.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.NodeEvent();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.type = reader.int32();
                        break;
                    }
                case 2: {
                        message.node = $root.registry.NodeInfo.decode(reader, reader.uint32());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a NodeEvent message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.NodeEvent
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.NodeEvent} NodeEvent
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeEvent.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a NodeEvent message.
         * @function verify
         * @memberof registry.NodeEvent
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        NodeEvent.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.type != null && message.hasOwnProperty("type"))
                switch (message.type) {
                default:
                    return "type: enum value expected";
                case 0:
                case 1:
                case 2:
                    break;
                }
            if (message.node != null && message.hasOwnProperty("node")) {
                var error = $root.registry.NodeInfo.verify(message.node);
                if (error)
                    return "node." + error;
            }
            return null;
        };

        /**
         * Creates a NodeEvent message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.NodeEvent
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.NodeEvent} NodeEvent
         */
        NodeEvent.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.NodeEvent)
                return object;
            var message = new $root.registry.NodeEvent();
            switch (object.type) {
            default:
                if (typeof object.type === "number") {
                    message.type = object.type;
                    break;
                }
                break;
            case "JOIN":
            case 0:
                message.type = 0;
                break;
            case "LEAVE":
            case 1:
                message.type = 1;
                break;
            case "UPDATE":
            case 2:
                message.type = 2;
                break;
            }
            if (object.node != null) {
                if (typeof object.node !== "object")
                    throw TypeError(".registry.NodeEvent.node: object expected");
                message.node = $root.registry.NodeInfo.fromObject(object.node);
            }
            return message;
        };

        /**
         * Creates a plain object from a NodeEvent message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.NodeEvent
         * @static
         * @param {registry.NodeEvent} message NodeEvent
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        NodeEvent.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.type = options.enums === String ? "JOIN" : 0;
                object.node = null;
            }
            if (message.type != null && message.hasOwnProperty("type"))
                object.type = options.enums === String ? $root.registry.NodeEvent.Type[message.type] === undefined ? message.type : $root.registry.NodeEvent.Type[message.type] : message.type;
            if (message.node != null && message.hasOwnProperty("node"))
                object.node = $root.registry.NodeInfo.toObject(message.node, options);
            return object;
        };

        /**
         * Converts this NodeEvent to JSON.
         * @function toJSON
         * @memberof registry.NodeEvent
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        NodeEvent.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for NodeEvent
         * @function getTypeUrl
         * @memberof registry.NodeEvent
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        NodeEvent.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.NodeEvent";
        };

        /**
         * Type enum.
         * @name registry.NodeEvent.Type
         * @enum {number}
         * @property {number} JOIN=0 JOIN value
         * @property {number} LEAVE=1 LEAVE value
         * @property {number} UPDATE=2 UPDATE value
         */
        NodeEvent.Type = (function() {
            var valuesById = {}, values = Object.create(valuesById);
            values[valuesById[0] = "JOIN"] = 0;
            values[valuesById[1] = "LEAVE"] = 1;
            values[valuesById[2] = "UPDATE"] = 2;
            return values;
        })();

        return NodeEvent;
    })();

    registry.Result = (function() {

        /**
         * Properties of a Result.
         * @memberof registry
         * @interface IResult
         * @property {boolean|null} [ok] Result ok
         * @property {number|null} [code] Result code
         * @property {string|null} [msg] Result msg
         */

        /**
         * Constructs a new Result.
         * @memberof registry
         * @classdesc Represents a Result.
         * @implements IResult
         * @constructor
         * @param {registry.IResult=} [properties] Properties to set
         */
        function Result(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * Result ok.
         * @member {boolean} ok
         * @memberof registry.Result
         * @instance
         */
        Result.prototype.ok = false;

        /**
         * Result code.
         * @member {number} code
         * @memberof registry.Result
         * @instance
         */
        Result.prototype.code = 0;

        /**
         * Result msg.
         * @member {string} msg
         * @memberof registry.Result
         * @instance
         */
        Result.prototype.msg = "";

        /**
         * Creates a new Result instance using the specified properties.
         * @function create
         * @memberof registry.Result
         * @static
         * @param {registry.IResult=} [properties] Properties to set
         * @returns {registry.Result} Result instance
         */
        Result.create = function create(properties) {
            return new Result(properties);
        };

        /**
         * Encodes the specified Result message. Does not implicitly {@link registry.Result.verify|verify} messages.
         * @function encode
         * @memberof registry.Result
         * @static
         * @param {registry.IResult} message Result message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Result.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.ok != null && Object.hasOwnProperty.call(message, "ok"))
                writer.uint32(/* id 1, wireType 0 =*/8).bool(message.ok);
            if (message.code != null && Object.hasOwnProperty.call(message, "code"))
                writer.uint32(/* id 2, wireType 0 =*/16).uint32(message.code);
            if (message.msg != null && Object.hasOwnProperty.call(message, "msg"))
                writer.uint32(/* id 3, wireType 2 =*/26).string(message.msg);
            return writer;
        };

        /**
         * Encodes the specified Result message, length delimited. Does not implicitly {@link registry.Result.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.Result
         * @static
         * @param {registry.IResult} message Result message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        Result.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a Result message from the specified reader or buffer.
         * @function decode
         * @memberof registry.Result
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.Result} Result
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Result.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.Result();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.ok = reader.bool();
                        break;
                    }
                case 2: {
                        message.code = reader.uint32();
                        break;
                    }
                case 3: {
                        message.msg = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a Result message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.Result
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.Result} Result
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        Result.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a Result message.
         * @function verify
         * @memberof registry.Result
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        Result.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.ok != null && message.hasOwnProperty("ok"))
                if (typeof message.ok !== "boolean")
                    return "ok: boolean expected";
            if (message.code != null && message.hasOwnProperty("code"))
                if (!$util.isInteger(message.code))
                    return "code: integer expected";
            if (message.msg != null && message.hasOwnProperty("msg"))
                if (!$util.isString(message.msg))
                    return "msg: string expected";
            return null;
        };

        /**
         * Creates a Result message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.Result
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.Result} Result
         */
        Result.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.Result)
                return object;
            var message = new $root.registry.Result();
            if (object.ok != null)
                message.ok = Boolean(object.ok);
            if (object.code != null)
                message.code = object.code >>> 0;
            if (object.msg != null)
                message.msg = String(object.msg);
            return message;
        };

        /**
         * Creates a plain object from a Result message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.Result
         * @static
         * @param {registry.Result} message Result
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        Result.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults) {
                object.ok = false;
                object.code = 0;
                object.msg = "";
            }
            if (message.ok != null && message.hasOwnProperty("ok"))
                object.ok = message.ok;
            if (message.code != null && message.hasOwnProperty("code"))
                object.code = message.code;
            if (message.msg != null && message.hasOwnProperty("msg"))
                object.msg = message.msg;
            return object;
        };

        /**
         * Converts this Result to JSON.
         * @function toJSON
         * @memberof registry.Result
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        Result.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for Result
         * @function getTypeUrl
         * @memberof registry.Result
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        Result.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.Result";
        };

        return Result;
    })();

    registry.NodeId = (function() {

        /**
         * Properties of a NodeId.
         * @memberof registry
         * @interface INodeId
         * @property {string|null} [nodeId] NodeId nodeId
         */

        /**
         * Constructs a new NodeId.
         * @memberof registry
         * @classdesc Represents a NodeId.
         * @implements INodeId
         * @constructor
         * @param {registry.INodeId=} [properties] Properties to set
         */
        function NodeId(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * NodeId nodeId.
         * @member {string} nodeId
         * @memberof registry.NodeId
         * @instance
         */
        NodeId.prototype.nodeId = "";

        /**
         * Creates a new NodeId instance using the specified properties.
         * @function create
         * @memberof registry.NodeId
         * @static
         * @param {registry.INodeId=} [properties] Properties to set
         * @returns {registry.NodeId} NodeId instance
         */
        NodeId.create = function create(properties) {
            return new NodeId(properties);
        };

        /**
         * Encodes the specified NodeId message. Does not implicitly {@link registry.NodeId.verify|verify} messages.
         * @function encode
         * @memberof registry.NodeId
         * @static
         * @param {registry.INodeId} message NodeId message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeId.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.nodeId != null && Object.hasOwnProperty.call(message, "nodeId"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.nodeId);
            return writer;
        };

        /**
         * Encodes the specified NodeId message, length delimited. Does not implicitly {@link registry.NodeId.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.NodeId
         * @static
         * @param {registry.INodeId} message NodeId message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        NodeId.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a NodeId message from the specified reader or buffer.
         * @function decode
         * @memberof registry.NodeId
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.NodeId} NodeId
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeId.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.NodeId();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.nodeId = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a NodeId message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.NodeId
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.NodeId} NodeId
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        NodeId.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a NodeId message.
         * @function verify
         * @memberof registry.NodeId
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        NodeId.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.nodeId != null && message.hasOwnProperty("nodeId"))
                if (!$util.isString(message.nodeId))
                    return "nodeId: string expected";
            return null;
        };

        /**
         * Creates a NodeId message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.NodeId
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.NodeId} NodeId
         */
        NodeId.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.NodeId)
                return object;
            var message = new $root.registry.NodeId();
            if (object.nodeId != null)
                message.nodeId = String(object.nodeId);
            return message;
        };

        /**
         * Creates a plain object from a NodeId message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.NodeId
         * @static
         * @param {registry.NodeId} message NodeId
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        NodeId.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.nodeId = "";
            if (message.nodeId != null && message.hasOwnProperty("nodeId"))
                object.nodeId = message.nodeId;
            return object;
        };

        /**
         * Converts this NodeId to JSON.
         * @function toJSON
         * @memberof registry.NodeId
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        NodeId.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for NodeId
         * @function getTypeUrl
         * @memberof registry.NodeId
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        NodeId.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.NodeId";
        };

        return NodeId;
    })();

    registry.ServiceType = (function() {

        /**
         * Properties of a ServiceType.
         * @memberof registry
         * @interface IServiceType
         * @property {string|null} [serviceType] ServiceType serviceType
         */

        /**
         * Constructs a new ServiceType.
         * @memberof registry
         * @classdesc Represents a ServiceType.
         * @implements IServiceType
         * @constructor
         * @param {registry.IServiceType=} [properties] Properties to set
         */
        function ServiceType(properties) {
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * ServiceType serviceType.
         * @member {string} serviceType
         * @memberof registry.ServiceType
         * @instance
         */
        ServiceType.prototype.serviceType = "";

        /**
         * Creates a new ServiceType instance using the specified properties.
         * @function create
         * @memberof registry.ServiceType
         * @static
         * @param {registry.IServiceType=} [properties] Properties to set
         * @returns {registry.ServiceType} ServiceType instance
         */
        ServiceType.create = function create(properties) {
            return new ServiceType(properties);
        };

        /**
         * Encodes the specified ServiceType message. Does not implicitly {@link registry.ServiceType.verify|verify} messages.
         * @function encode
         * @memberof registry.ServiceType
         * @static
         * @param {registry.IServiceType} message ServiceType message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ServiceType.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.serviceType != null && Object.hasOwnProperty.call(message, "serviceType"))
                writer.uint32(/* id 1, wireType 2 =*/10).string(message.serviceType);
            return writer;
        };

        /**
         * Encodes the specified ServiceType message, length delimited. Does not implicitly {@link registry.ServiceType.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.ServiceType
         * @static
         * @param {registry.IServiceType} message ServiceType message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        ServiceType.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a ServiceType message from the specified reader or buffer.
         * @function decode
         * @memberof registry.ServiceType
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.ServiceType} ServiceType
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ServiceType.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.ServiceType();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        message.serviceType = reader.string();
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a ServiceType message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.ServiceType
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.ServiceType} ServiceType
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        ServiceType.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a ServiceType message.
         * @function verify
         * @memberof registry.ServiceType
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        ServiceType.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.serviceType != null && message.hasOwnProperty("serviceType"))
                if (!$util.isString(message.serviceType))
                    return "serviceType: string expected";
            return null;
        };

        /**
         * Creates a ServiceType message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.ServiceType
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.ServiceType} ServiceType
         */
        ServiceType.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.ServiceType)
                return object;
            var message = new $root.registry.ServiceType();
            if (object.serviceType != null)
                message.serviceType = String(object.serviceType);
            return message;
        };

        /**
         * Creates a plain object from a ServiceType message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.ServiceType
         * @static
         * @param {registry.ServiceType} message ServiceType
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        ServiceType.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.defaults)
                object.serviceType = "";
            if (message.serviceType != null && message.hasOwnProperty("serviceType"))
                object.serviceType = message.serviceType;
            return object;
        };

        /**
         * Converts this ServiceType to JSON.
         * @function toJSON
         * @memberof registry.ServiceType
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        ServiceType.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for ServiceType
         * @function getTypeUrl
         * @memberof registry.ServiceType
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        ServiceType.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.ServiceType";
        };

        return ServiceType;
    })();

    registry.SubscribeReq = (function() {

        /**
         * Properties of a SubscribeReq.
         * @memberof registry
         * @interface ISubscribeReq
         * @property {Array.<string>|null} [serviceTypes] SubscribeReq serviceTypes
         */

        /**
         * Constructs a new SubscribeReq.
         * @memberof registry
         * @classdesc Represents a SubscribeReq.
         * @implements ISubscribeReq
         * @constructor
         * @param {registry.ISubscribeReq=} [properties] Properties to set
         */
        function SubscribeReq(properties) {
            this.serviceTypes = [];
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * SubscribeReq serviceTypes.
         * @member {Array.<string>} serviceTypes
         * @memberof registry.SubscribeReq
         * @instance
         */
        SubscribeReq.prototype.serviceTypes = $util.emptyArray;

        /**
         * Creates a new SubscribeReq instance using the specified properties.
         * @function create
         * @memberof registry.SubscribeReq
         * @static
         * @param {registry.ISubscribeReq=} [properties] Properties to set
         * @returns {registry.SubscribeReq} SubscribeReq instance
         */
        SubscribeReq.create = function create(properties) {
            return new SubscribeReq(properties);
        };

        /**
         * Encodes the specified SubscribeReq message. Does not implicitly {@link registry.SubscribeReq.verify|verify} messages.
         * @function encode
         * @memberof registry.SubscribeReq
         * @static
         * @param {registry.ISubscribeReq} message SubscribeReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        SubscribeReq.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.serviceTypes != null && message.serviceTypes.length)
                for (var i = 0; i < message.serviceTypes.length; ++i)
                    writer.uint32(/* id 1, wireType 2 =*/10).string(message.serviceTypes[i]);
            return writer;
        };

        /**
         * Encodes the specified SubscribeReq message, length delimited. Does not implicitly {@link registry.SubscribeReq.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.SubscribeReq
         * @static
         * @param {registry.ISubscribeReq} message SubscribeReq message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        SubscribeReq.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a SubscribeReq message from the specified reader or buffer.
         * @function decode
         * @memberof registry.SubscribeReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.SubscribeReq} SubscribeReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        SubscribeReq.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.SubscribeReq();
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        if (!(message.serviceTypes && message.serviceTypes.length))
                            message.serviceTypes = [];
                        message.serviceTypes.push(reader.string());
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a SubscribeReq message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.SubscribeReq
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.SubscribeReq} SubscribeReq
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        SubscribeReq.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a SubscribeReq message.
         * @function verify
         * @memberof registry.SubscribeReq
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        SubscribeReq.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.serviceTypes != null && message.hasOwnProperty("serviceTypes")) {
                if (!Array.isArray(message.serviceTypes))
                    return "serviceTypes: array expected";
                for (var i = 0; i < message.serviceTypes.length; ++i)
                    if (!$util.isString(message.serviceTypes[i]))
                        return "serviceTypes: string[] expected";
            }
            return null;
        };

        /**
         * Creates a SubscribeReq message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.SubscribeReq
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.SubscribeReq} SubscribeReq
         */
        SubscribeReq.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.SubscribeReq)
                return object;
            var message = new $root.registry.SubscribeReq();
            if (object.serviceTypes) {
                if (!Array.isArray(object.serviceTypes))
                    throw TypeError(".registry.SubscribeReq.serviceTypes: array expected");
                message.serviceTypes = [];
                for (var i = 0; i < object.serviceTypes.length; ++i)
                    message.serviceTypes[i] = String(object.serviceTypes[i]);
            }
            return message;
        };

        /**
         * Creates a plain object from a SubscribeReq message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.SubscribeReq
         * @static
         * @param {registry.SubscribeReq} message SubscribeReq
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        SubscribeReq.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.arrays || options.defaults)
                object.serviceTypes = [];
            if (message.serviceTypes && message.serviceTypes.length) {
                object.serviceTypes = [];
                for (var j = 0; j < message.serviceTypes.length; ++j)
                    object.serviceTypes[j] = message.serviceTypes[j];
            }
            return object;
        };

        /**
         * Converts this SubscribeReq to JSON.
         * @function toJSON
         * @memberof registry.SubscribeReq
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        SubscribeReq.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for SubscribeReq
         * @function getTypeUrl
         * @memberof registry.SubscribeReq
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        SubscribeReq.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.SubscribeReq";
        };

        return SubscribeReq;
    })();

    registry.SubscribeRes = (function() {

        /**
         * Properties of a SubscribeRes.
         * @memberof registry
         * @interface ISubscribeRes
         * @property {Object.<string,registry.INodeList>|null} [snapshot] SubscribeRes snapshot
         */

        /**
         * Constructs a new SubscribeRes.
         * @memberof registry
         * @classdesc Represents a SubscribeRes.
         * @implements ISubscribeRes
         * @constructor
         * @param {registry.ISubscribeRes=} [properties] Properties to set
         */
        function SubscribeRes(properties) {
            this.snapshot = {};
            if (properties)
                for (var keys = Object.keys(properties), i = 0; i < keys.length; ++i)
                    if (properties[keys[i]] != null)
                        this[keys[i]] = properties[keys[i]];
        }

        /**
         * SubscribeRes snapshot.
         * @member {Object.<string,registry.INodeList>} snapshot
         * @memberof registry.SubscribeRes
         * @instance
         */
        SubscribeRes.prototype.snapshot = $util.emptyObject;

        /**
         * Creates a new SubscribeRes instance using the specified properties.
         * @function create
         * @memberof registry.SubscribeRes
         * @static
         * @param {registry.ISubscribeRes=} [properties] Properties to set
         * @returns {registry.SubscribeRes} SubscribeRes instance
         */
        SubscribeRes.create = function create(properties) {
            return new SubscribeRes(properties);
        };

        /**
         * Encodes the specified SubscribeRes message. Does not implicitly {@link registry.SubscribeRes.verify|verify} messages.
         * @function encode
         * @memberof registry.SubscribeRes
         * @static
         * @param {registry.ISubscribeRes} message SubscribeRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        SubscribeRes.encode = function encode(message, writer) {
            if (!writer)
                writer = $Writer.create();
            if (message.snapshot != null && Object.hasOwnProperty.call(message, "snapshot"))
                for (var keys = Object.keys(message.snapshot), i = 0; i < keys.length; ++i) {
                    writer.uint32(/* id 1, wireType 2 =*/10).fork().uint32(/* id 1, wireType 2 =*/10).string(keys[i]);
                    $root.registry.NodeList.encode(message.snapshot[keys[i]], writer.uint32(/* id 2, wireType 2 =*/18).fork()).ldelim().ldelim();
                }
            return writer;
        };

        /**
         * Encodes the specified SubscribeRes message, length delimited. Does not implicitly {@link registry.SubscribeRes.verify|verify} messages.
         * @function encodeDelimited
         * @memberof registry.SubscribeRes
         * @static
         * @param {registry.ISubscribeRes} message SubscribeRes message or plain object to encode
         * @param {$protobuf.Writer} [writer] Writer to encode to
         * @returns {$protobuf.Writer} Writer
         */
        SubscribeRes.encodeDelimited = function encodeDelimited(message, writer) {
            return this.encode(message, writer).ldelim();
        };

        /**
         * Decodes a SubscribeRes message from the specified reader or buffer.
         * @function decode
         * @memberof registry.SubscribeRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @param {number} [length] Message length if known beforehand
         * @returns {registry.SubscribeRes} SubscribeRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        SubscribeRes.decode = function decode(reader, length, error) {
            if (!(reader instanceof $Reader))
                reader = $Reader.create(reader);
            var end = length === undefined ? reader.len : reader.pos + length, message = new $root.registry.SubscribeRes(), key, value;
            while (reader.pos < end) {
                var tag = reader.uint32();
                if (tag === error)
                    break;
                switch (tag >>> 3) {
                case 1: {
                        if (message.snapshot === $util.emptyObject)
                            message.snapshot = {};
                        var end2 = reader.uint32() + reader.pos;
                        key = "";
                        value = null;
                        while (reader.pos < end2) {
                            var tag2 = reader.uint32();
                            switch (tag2 >>> 3) {
                            case 1:
                                key = reader.string();
                                break;
                            case 2:
                                value = $root.registry.NodeList.decode(reader, reader.uint32());
                                break;
                            default:
                                reader.skipType(tag2 & 7);
                                break;
                            }
                        }
                        message.snapshot[key] = value;
                        break;
                    }
                default:
                    reader.skipType(tag & 7);
                    break;
                }
            }
            return message;
        };

        /**
         * Decodes a SubscribeRes message from the specified reader or buffer, length delimited.
         * @function decodeDelimited
         * @memberof registry.SubscribeRes
         * @static
         * @param {$protobuf.Reader|Uint8Array} reader Reader or buffer to decode from
         * @returns {registry.SubscribeRes} SubscribeRes
         * @throws {Error} If the payload is not a reader or valid buffer
         * @throws {$protobuf.util.ProtocolError} If required fields are missing
         */
        SubscribeRes.decodeDelimited = function decodeDelimited(reader) {
            if (!(reader instanceof $Reader))
                reader = new $Reader(reader);
            return this.decode(reader, reader.uint32());
        };

        /**
         * Verifies a SubscribeRes message.
         * @function verify
         * @memberof registry.SubscribeRes
         * @static
         * @param {Object.<string,*>} message Plain object to verify
         * @returns {string|null} `null` if valid, otherwise the reason why it is not
         */
        SubscribeRes.verify = function verify(message) {
            if (typeof message !== "object" || message === null)
                return "object expected";
            if (message.snapshot != null && message.hasOwnProperty("snapshot")) {
                if (!$util.isObject(message.snapshot))
                    return "snapshot: object expected";
                var key = Object.keys(message.snapshot);
                for (var i = 0; i < key.length; ++i) {
                    var error = $root.registry.NodeList.verify(message.snapshot[key[i]]);
                    if (error)
                        return "snapshot." + error;
                }
            }
            return null;
        };

        /**
         * Creates a SubscribeRes message from a plain object. Also converts values to their respective internal types.
         * @function fromObject
         * @memberof registry.SubscribeRes
         * @static
         * @param {Object.<string,*>} object Plain object
         * @returns {registry.SubscribeRes} SubscribeRes
         */
        SubscribeRes.fromObject = function fromObject(object) {
            if (object instanceof $root.registry.SubscribeRes)
                return object;
            var message = new $root.registry.SubscribeRes();
            if (object.snapshot) {
                if (typeof object.snapshot !== "object")
                    throw TypeError(".registry.SubscribeRes.snapshot: object expected");
                message.snapshot = {};
                for (var keys = Object.keys(object.snapshot), i = 0; i < keys.length; ++i) {
                    if (typeof object.snapshot[keys[i]] !== "object")
                        throw TypeError(".registry.SubscribeRes.snapshot: object expected");
                    message.snapshot[keys[i]] = $root.registry.NodeList.fromObject(object.snapshot[keys[i]]);
                }
            }
            return message;
        };

        /**
         * Creates a plain object from a SubscribeRes message. Also converts values to other types if specified.
         * @function toObject
         * @memberof registry.SubscribeRes
         * @static
         * @param {registry.SubscribeRes} message SubscribeRes
         * @param {$protobuf.IConversionOptions} [options] Conversion options
         * @returns {Object.<string,*>} Plain object
         */
        SubscribeRes.toObject = function toObject(message, options) {
            if (!options)
                options = {};
            var object = {};
            if (options.objects || options.defaults)
                object.snapshot = {};
            var keys2;
            if (message.snapshot && (keys2 = Object.keys(message.snapshot)).length) {
                object.snapshot = {};
                for (var j = 0; j < keys2.length; ++j)
                    object.snapshot[keys2[j]] = $root.registry.NodeList.toObject(message.snapshot[keys2[j]], options);
            }
            return object;
        };

        /**
         * Converts this SubscribeRes to JSON.
         * @function toJSON
         * @memberof registry.SubscribeRes
         * @instance
         * @returns {Object.<string,*>} JSON object
         */
        SubscribeRes.prototype.toJSON = function toJSON() {
            return this.constructor.toObject(this, $protobuf.util.toJSONOptions);
        };

        /**
         * Gets the default type url for SubscribeRes
         * @function getTypeUrl
         * @memberof registry.SubscribeRes
         * @static
         * @param {string} [typeUrlPrefix] your custom typeUrlPrefix(default "type.googleapis.com")
         * @returns {string} The default type url
         */
        SubscribeRes.getTypeUrl = function getTypeUrl(typeUrlPrefix) {
            if (typeUrlPrefix === undefined) {
                typeUrlPrefix = "type.googleapis.com";
            }
            return typeUrlPrefix + "/registry.SubscribeRes";
        };

        return SubscribeRes;
    })();

    registry.Registry = (function() {

        /**
         * Constructs a new Registry service.
         * @memberof registry
         * @classdesc Represents a Registry
         * @extends $protobuf.rpc.Service
         * @constructor
         * @param {$protobuf.RPCImpl} rpcImpl RPC implementation
         * @param {boolean} [requestDelimited=false] Whether requests are length-delimited
         * @param {boolean} [responseDelimited=false] Whether responses are length-delimited
         */
        function Registry(rpcImpl, requestDelimited, responseDelimited) {
            $protobuf.rpc.Service.call(this, rpcImpl, requestDelimited, responseDelimited);
        }

        (Registry.prototype = Object.create($protobuf.rpc.Service.prototype)).constructor = Registry;

        /**
         * Creates new Registry service using the specified rpc implementation.
         * @function create
         * @memberof registry.Registry
         * @static
         * @param {$protobuf.RPCImpl} rpcImpl RPC implementation
         * @param {boolean} [requestDelimited=false] Whether requests are length-delimited
         * @param {boolean} [responseDelimited=false] Whether responses are length-delimited
         * @returns {Registry} RPC service. Useful where requests and/or responses are streamed.
         */
        Registry.create = function create(rpcImpl, requestDelimited, responseDelimited) {
            return new this(rpcImpl, requestDelimited, responseDelimited);
        };

        /**
         * Callback as used by {@link registry.Registry#register}.
         * @memberof registry.Registry
         * @typedef RegisterCallback
         * @type {function}
         * @param {Error|null} error Error, if any
         * @param {registry.Result} [response] Result
         */

        /**
         * Calls Register.
         * @function register
         * @memberof registry.Registry
         * @instance
         * @param {registry.INodeInfo} request NodeInfo message or plain object
         * @param {registry.Registry.RegisterCallback} callback Node-style callback called with the error, if any, and Result
         * @returns {undefined}
         * @variation 1
         */
        Object.defineProperty(Registry.prototype.register = function register(request, callback) {
            return this.rpcCall(register, $root.registry.NodeInfo, $root.registry.Result, request, callback);
        }, "name", { value: "Register" });

        /**
         * Calls Register.
         * @function register
         * @memberof registry.Registry
         * @instance
         * @param {registry.INodeInfo} request NodeInfo message or plain object
         * @returns {Promise<registry.Result>} Promise
         * @variation 2
         */

        /**
         * Callback as used by {@link registry.Registry#heartbeat}.
         * @memberof registry.Registry
         * @typedef HeartbeatCallback
         * @type {function}
         * @param {Error|null} error Error, if any
         * @param {registry.Result} [response] Result
         */

        /**
         * Calls Heartbeat.
         * @function heartbeat
         * @memberof registry.Registry
         * @instance
         * @param {registry.INodeId} request NodeId message or plain object
         * @param {registry.Registry.HeartbeatCallback} callback Node-style callback called with the error, if any, and Result
         * @returns {undefined}
         * @variation 1
         */
        Object.defineProperty(Registry.prototype.heartbeat = function heartbeat(request, callback) {
            return this.rpcCall(heartbeat, $root.registry.NodeId, $root.registry.Result, request, callback);
        }, "name", { value: "Heartbeat" });

        /**
         * Calls Heartbeat.
         * @function heartbeat
         * @memberof registry.Registry
         * @instance
         * @param {registry.INodeId} request NodeId message or plain object
         * @returns {Promise<registry.Result>} Promise
         * @variation 2
         */

        /**
         * Callback as used by {@link registry.Registry#discover}.
         * @memberof registry.Registry
         * @typedef DiscoverCallback
         * @type {function}
         * @param {Error|null} error Error, if any
         * @param {registry.NodeList} [response] NodeList
         */

        /**
         * Calls Discover.
         * @function discover
         * @memberof registry.Registry
         * @instance
         * @param {registry.IServiceType} request ServiceType message or plain object
         * @param {registry.Registry.DiscoverCallback} callback Node-style callback called with the error, if any, and NodeList
         * @returns {undefined}
         * @variation 1
         */
        Object.defineProperty(Registry.prototype.discover = function discover(request, callback) {
            return this.rpcCall(discover, $root.registry.ServiceType, $root.registry.NodeList, request, callback);
        }, "name", { value: "Discover" });

        /**
         * Calls Discover.
         * @function discover
         * @memberof registry.Registry
         * @instance
         * @param {registry.IServiceType} request ServiceType message or plain object
         * @returns {Promise<registry.NodeList>} Promise
         * @variation 2
         */

        /**
         * Callback as used by {@link registry.Registry#watch}.
         * @memberof registry.Registry
         * @typedef WatchCallback
         * @type {function}
         * @param {Error|null} error Error, if any
         * @param {registry.NodeEvent} [response] NodeEvent
         */

        /**
         * Calls Watch.
         * @function watch
         * @memberof registry.Registry
         * @instance
         * @param {registry.IServiceType} request ServiceType message or plain object
         * @param {registry.Registry.WatchCallback} callback Node-style callback called with the error, if any, and NodeEvent
         * @returns {undefined}
         * @variation 1
         */
        Object.defineProperty(Registry.prototype.watch = function watch(request, callback) {
            return this.rpcCall(watch, $root.registry.ServiceType, $root.registry.NodeEvent, request, callback);
        }, "name", { value: "Watch" });

        /**
         * Calls Watch.
         * @function watch
         * @memberof registry.Registry
         * @instance
         * @param {registry.IServiceType} request ServiceType message or plain object
         * @returns {Promise<registry.NodeEvent>} Promise
         * @variation 2
         */

        /**
         * Callback as used by {@link registry.Registry#subscribe}.
         * @memberof registry.Registry
         * @typedef SubscribeCallback
         * @type {function}
         * @param {Error|null} error Error, if any
         * @param {registry.SubscribeRes} [response] SubscribeRes
         */

        /**
         * Calls Subscribe.
         * @function subscribe
         * @memberof registry.Registry
         * @instance
         * @param {registry.ISubscribeReq} request SubscribeReq message or plain object
         * @param {registry.Registry.SubscribeCallback} callback Node-style callback called with the error, if any, and SubscribeRes
         * @returns {undefined}
         * @variation 1
         */
        Object.defineProperty(Registry.prototype.subscribe = function subscribe(request, callback) {
            return this.rpcCall(subscribe, $root.registry.SubscribeReq, $root.registry.SubscribeRes, request, callback);
        }, "name", { value: "Subscribe" });

        /**
         * Calls Subscribe.
         * @function subscribe
         * @memberof registry.Registry
         * @instance
         * @param {registry.ISubscribeReq} request SubscribeReq message or plain object
         * @returns {Promise<registry.SubscribeRes>} Promise
         * @variation 2
         */

        return Registry;
    })();

    return registry;
})();

module.exports = $root;
