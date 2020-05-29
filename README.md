# YubiKey OTP for node.js

This is an implementation of YubiKey challenge-response OTP for node.js

## Features

- fast native implementation using yubico-c and ykpers
- non-blocking API, I/O is performed in a separate thread
- thread-safe library

## Installation

The package can be installed from npm:

```sh
npm i yubikey-otp
```

## Usage

```javascript
const ykotp = require('yubikey-otp');

// Get a list of connected YubiKeys
ykotp.getYubiKeys((err, yubiKeys) => {
    // Select one of them and pass it to the challenge-response function
    const yubiKey = yubiKeys[0];

    const challenge = Buffer.from('aa', 'hex');
    const slot = 2;

    ykotp.challengeResponse(yubiKey, challenge, slot, (err, response) => {
        if (err) {
            if (err.touchRequested) {
                // Present the touch UI or show a message in the terminal
                // NOTE: the same callback will be called once again after it
                console.log('Please touch your YubiKey');
            } else {
                // There was some error
            }
        } else {
            // Response calculated successfully
            console.log('Response', response.toString('hex'));
        }
    });
});
```

## Reference

### `version`

Returns the version of the library

```javascript
> ykotp.version
'0.0.1'
```

### `ykpersVersion`

Returns the version of `ykpers`, the underlying low-level YubiKey API

```javascript
> ykotp.version
'0.0.1'
```

### `getYubiKeys`

Gets the list of connected YubiKeys

Arguments:
- `callback`: callback that will be called once the list is retrieved

```javascript
> ykotp.getYubiKeys((err, yubiKeys) => console.log(yubiKeys))
[
  {
    serial: 12345678,
    vid: 4176,
    pid: 1031,
    version: '5.2.4',
    slot1: true,
    slot2: true
  }
]
```

### `challengeResponse`

Calculates the challenge-response, assuming it's programmed into the given slot

Arguments:
- `yubiKey`: object representing the YubiKey with these properties:
    - vid
    - pid
    - serial
- `challenge`: node.js `Buffer` with the desired challenge
- `slot`: one of two slots: `1` or `2`
- `callback`: result callback that will be called:
    - in case of error, with `(error)`
    - in case of success, with `(undefined, Buffer)`
    - when a touch gesture is requested, with `({ touchRequested: true })`.
        In this case the same callback will be called once again in the end!

```javascript
> ykotp.challengeResponse({ vid: 4176, pid: 1031, serial: 12345678 },
    Buffer.from('aa', 'hex'), 2, (err, result) => console.log(err, result))
[Error: Touch requested] { touchRequested: true } undefined
undefined <Buffer ...>
```

## Development

Clone the repo with submodules:

```sh
git clone git@github.com:antelle/node-yubikey-otp.git --recursive
```

Build `yubikey-personalization` ([official instructions](https://github.com/Yubico/yubikey-personalization#building-from-git)):

```sh
cd yubikey-personalization
autoreconf --install
./configure
make check install
cd ..
```

Build `yubico-c` ([official guide](https://github.com/Yubico/yubico-c#building)):

```sh
cd yubico-c
autoreconf --install
./configure
make check
cd ..
```

Finally build the addon:

```sh
npm run build
```

Insert your YubiKey and check if it's working:
```sh
npm run test
```

## License

MIT
