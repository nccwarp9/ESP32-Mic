// Remote server

var PORT = 8080;
var HOST = '192.168.1.9'

var samplesLength = 1000;
var sampleRate = 16000;
var bitsPerSample = 8;

var numChannels = 1;

var fs = require("fs");
var outStream = fs.createWriteStream("test.wav");

var writeHeader = function() {
	var b = new Buffer(1024);
	b.write('RIFF', 0);
	/* file length */
	b.writeUInt32LE(32 + samplesLength * numChannels, 4);
	//b.writeUint32LE(0, 4);

	b.write('WAVE', 8);

	/* format chunk identifier */
	b.write('fmt ', 12);

	/* format chunk length */
	b.writeUInt32LE(16, 16);

	/* sample format (raw) */
	b.writeUInt16LE(1, 20);

	/* channel count */
	b.writeUInt16LE(1, 22);

	/* sample rate */
	b.writeUInt32LE(sampleRate, 24);

	/* byte rate (sample rate * block align) */
	b.writeUInt32LE(sampleRate * 1, 28);
	//b.writeUInt32LE(sampleRate * 2, 28);

	/* block align (channel count * bytes per sample) */
	b.writeUInt16LE(numChannels * 1, 32);
	//b.writeUInt16LE(2, 32);

	/* bits per sample */
	b.writeUInt16LE(bitsPerSample, 34);

	/* data chunk identifier */
	b.write('data', 36);

	/* data chunk length */
	//b.writeUInt32LE(40, samplesLength * 2);
	b.writeUInt32LE(0, 40);


	outStream.write(b.slice(0, 50));
};

writeHeader(outStream);


var dgram = require('dgram');
var server = dgram.createSocket('udp4');

server.on('listening', function () {
    var address = server.address();
    console.log('UDP Server listening on ' + address.address + ":" + address.port);
});

server.on('message', function (message, remote) {
    console.log('Message received')
    outStream.write(message);
})

server.bind(PORT);

setTimeout(function() {
	console.log('recorded for 10 seconds');
	outStream.end();
	process.exit(0);
}, 10 * 1000);