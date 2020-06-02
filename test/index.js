const { inspect } = require('util');
const {
    version,
    getYubiKeys,
    challengeResponse,
    cancelChallengeResponse,
} = require('../');

console.log(`yubikey-chalresp v${version}`);

const yubiKeys = getYubiKeys({
    // getCapabilities: true,
    // testSlots: true
}, (err, yubiKeys) => {
    if (err) {
        return console.log(err);
    }
    console.log(inspect(yubiKeys, { depth: null }));

    const yubiKey = yubiKeys[0];
    const challenge = Buffer.from('aa', 'hex');

    if (yubiKey) {
        let touchTimeout;
        challengeResponse(yubiKey, challenge, 2, (e, response) => {
            if (e) {
                if (e.touchRequested) {
                    console.log(`Please touch YubiKey ${yubiKey.serial}`);
                    touchTimeout = setTimeout(() => {
                        console.log(`Sorry, it's too late`);
                        cancelChallengeResponse();
                    }, 3000);
                } else {
                    clearTimeout(touchTimeout);
                    console.log(e);
                }
            } else {
                clearTimeout(touchTimeout);
                console.log('Response', response.toString('hex'));
            }
        });
    } else {
        console.log('No YubiKeys');
    }
});
