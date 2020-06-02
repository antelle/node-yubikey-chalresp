# YubiKey challenge-response for node.js

This is an implementation of YubiKey challenge-response OTP for node.js

## Features

- fast native implementation using yubico-c and ykpers
- non-blocking API, I/O is performed in a separate thread
- thread-safe library, locking is done inside
- no additional JavaScript, all you need is the `.node` file
- no runtime dependencies
- possibility to cancel pending challenge-response

## Installation

If you're on Linux, install `libusb-1.0` dependency first:

```sh
apt-get install libusb-1.0-0-dev
```

Install the package from npm:

```sh
npm i yubikey-chalresp
```

## Usage

```javascript
const ykchalresp = require('yubikey-chalresp');

// Get a list of connected YubiKeys
ykchalresp.getYubiKeys((err, yubiKeys) => {
    // Select one of them and pass it to the challenge-response function
    const yubiKey = yubiKeys[0];

    const challenge = Buffer.from('aa', 'hex');
    const slot = 2;

    ykchalresp.challengeResponse(yubiKey, challenge, slot, (err, response) => {
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
> ykchalresp.version
'0.0.1'
```

### `getYubiKeys`

Gets the list of connected YubiKeys

Arguments:
- `options`: object with possible parameters:
    - `getCapabilities`: read `capabilities` from YubiKeys
    - `testSlots`: perform a test challenge instead of relying on the config
- `callback`: callback that will be called once the list is retrieved

```javascript
const options = { getCapabilities: true, testSlots: true };
> ykchalresp.getYubiKeys(options, (err, yubiKeys) => console.log(yubiKeys))
[
  {
    serial: 12345678,
    vid: 4176,
    pid: 1031,
    version: '5.2.4',
    slots: [
      { number: 1, valid: true, touch: true },
      { number: 2, valid: true, touch: false }
    ]
    capabilities: <Buffer ...>
  }
]
```

`capabilities` are not always there, some YubiKeys don't provide them.

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
> ykchalresp.challengeResponse({ vid: 4176, pid: 1031, serial: 12345678 },
    Buffer.from('aa', 'hex'), 2, (err, result) => console.log(err, result))
[Error: Touch requested] { touchRequested: true } undefined
undefined <Buffer ...>
```

### `cancelChallengeResponse`

Cancels pending challenge-response operation and makes the YubiKey stop blinking.
Since we're always taking an exclusive lock, there can be only one challenge-response 
in progress, so there are no extra parameters here.

The function always succeeds and doesn't reutrn any value, the result is handled in async way.
Please note that due to race conditions you may still get a successful response if a YubiKey was pressed at the same moment. If the operation was successfully canceled, 
you will get an error with `YK_ETIMEOUT` error code.

### Error codes

If an error was triggered by the YubiKey or Yubico API, it will contain the error code 
reported by the library. The full list of error codes can be found 
[here](src/addon.cpp#L25). Errors can be checked like this:

```javascript
ykchalresp.challengeResponse(yk, challenge, slot, (err, result) => {
    if (err && err.code === ykchalresp.YK_ENOKEY) {
        console.log('Please insert your YubiKey');
    }
});
```

## Development

Clone the repo with submodules:

```sh
git clone git@github.com:antelle/node-yubikey-chalresp.git --recursive
```

Build the addon:

```sh
npm run rebuild
```

Insert your YubiKey and check if it's working:

```sh
npm run test
```

Iterative one-liner, convenient when you're testing something:

```sh
npm run build-test
```

## License

MIT
