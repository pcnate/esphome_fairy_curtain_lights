import { readFileSync, statSync, createReadStream } from 'node:fs';
import { resolve } from 'node:path';
import { request } from 'node:http';


const DEVICE_NAME = 'curtain-lights-dev';

const root = resolve( __dirname, '..' );
const envFile = resolve( root, '.env' );
const firmware = resolve( root, `examples/.esphome/build/${ DEVICE_NAME }/.pioenvs/${ DEVICE_NAME }/firmware.ota.bin` );

/** load .env and pull the device IP */
const env = readFileSync( envFile, 'utf-8' );
const match = env.match( /^ESPHOME_DEVICE_IP=(.+)$/m );

if ( !match ) {
  console.error( 'ESPHOME_DEVICE_IP not found in .env' );
  process.exit( 1 );
}

const ip = match[ 1 ].trim();
const stat = statSync( firmware );
const fileSize = stat.size;
const boundary = `----fairyupload${ Date.now() }`;

const preamble = Buffer.from(
  `--${ boundary }\r\n` +
  `Content-Disposition: form-data; name="update"; filename="firmware.ota.bin"\r\n` +
  `Content-Type: application/octet-stream\r\n\r\n`
);
const postamble = Buffer.from( `\r\n--${ boundary }--\r\n` );
const totalBytes = preamble.length + fileSize + postamble.length;


/**
 * Render a progress bar that fills the terminal width.
 *
 * @param sent - bytes uploaded so far
 * @param total - total bytes to upload
 */
function renderBar( sent: number, total: number ): void {
  const pct = total > 0 ? sent / total : 0;
  const sentKB = ( sent / 1024 ).toFixed( 1 );
  const totalKB = ( total / 1024 ).toFixed( 1 );
  const suffix = ` ${ ( pct * 100 ).toFixed( 1 ).padStart( 5 ) }%  ${ sentKB }/${ totalKB } KB`;

  const termWidth = process.stdout.columns || 80;
  const barWidth = Math.max( 10, termWidth - suffix.length - 3 );
  const filled = Math.round( pct * barWidth );
  const bar = '='.repeat( filled ) + '-'.repeat( barWidth - filled );

  process.stdout.write( `\r[${ bar }]${ suffix }` );
}


console.log( `=== Uploading firmware to ${ ip } (${ ( fileSize / 1024 ).toFixed( 1 ) } KB) ===` );

const req = request({
  host: ip,
  port: 80,
  method: 'POST',
  path: '/update',
  headers: {
    'Content-Type': `multipart/form-data; boundary=${ boundary }`,
    'Content-Length': totalBytes,
  },
}, ( res ) => {
  clearInterval( progressTimer );
  renderBar( totalBytes, totalBytes );
  const chunks: Buffer[] = [];
  res.on( 'data', ( c ) => chunks.push( c ) );
  res.on( 'end', () => {
    process.stdout.write( '\n' );
    const body = Buffer.concat( chunks ).toString();
    if ( res.statusCode && res.statusCode >= 200 && res.statusCode < 300 ) {
      console.log( `=== Upload complete (HTTP ${ res.statusCode }) ===` );
      if ( body.trim() ) console.log( body );
    } else {
      console.error( `Upload failed: HTTP ${ res.statusCode }` );
      if ( body.trim() ) console.error( body );
      process.exit( 1 );
    }
  });
});

req.on( 'error', ( err ) => {
  clearInterval( progressTimer );
  process.stdout.write( '\n' );
  console.error( `Upload error: ${ err.message }` );
  process.exit( 1 );
});

const progressTimer = setInterval( () => {
  const socket = req.socket;
  if ( socket ) {
    const sent = Math.min( socket.bytesWritten, totalBytes );
    renderBar( sent, totalBytes );
  }
}, 100 );

req.write( preamble );

const fileStream = createReadStream( firmware );
fileStream.on( 'data', ( chunk ) => {
  const buf = chunk as Buffer;
  if ( !req.write( buf ) ) {
    fileStream.pause();
    req.once( 'drain', () => fileStream.resume() );
  }
});
fileStream.on( 'end', () => {
  req.write( postamble );
  req.end();
});
fileStream.on( 'error', ( err ) => {
  clearInterval( progressTimer );
  process.stdout.write( '\n' );
  console.error( `File read error: ${ err.message }` );
  process.exit( 1 );
});
