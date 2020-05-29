const {
    version,
    ykpersVersion,
    getYubiKeys,
    challengeResponse
} = require('../');

console.log(`node-yubikey-otp v${version}, ykpers v${ykpersVersion}`);

const yubiKeys = getYubiKeys((err, yubiKeys) => {
    if (err) {
        return console.log(err);
    }
    console.log(yubiKeys);

    const yubiKey = yubiKeys[0];
    const challenge = Buffer.from('aa', 'hex');

    if (yubiKey) {
        challengeResponse(yubiKey, challenge, 2, (e, response) => {
            if (e) {
                if (e.touchRequested) {
                    console.log(`Please touch YubiKey ${yubiKey.serial}`);
                } else {
                    console.log(e);
                }
            } else {
                console.log('Response', response.toString('hex'));
            }
        });
    } else {
        console.log('No YubiKeys');
    }
});
