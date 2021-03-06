import { Schnorr } from './index';
import { TextEncoder } from 'util';
import { BarretenbergWorker } from '../../wasm/worker';
import { fetchCode } from '../../wasm';
import { createWorker, destroyWorker } from '../../wasm/worker_factory';

describe('schnorr', () => {
  let barretenberg!: BarretenbergWorker;
  let schnorr!: Schnorr;

  beforeAll(async () => {
    barretenberg = await createWorker();
    const code = await fetchCode();
    const module = new WebAssembly.Module(code);
    await barretenberg.init(module);
    schnorr = new Schnorr(barretenberg);
  });

  afterAll(async () => {
    await destroyWorker(barretenberg);
  });

  it('should verify signature', async () => {
    // prettier-ignore
    const pk = Buffer.from([
      0x0b, 0x9b, 0x3a, 0xde, 0xe6, 0xb3, 0xd8, 0x1b, 0x28, 0xa0, 0x88, 0x6b, 0x2a, 0x84, 0x15, 0xc7,
      0xda, 0x31, 0x29, 0x1a, 0x5e, 0x96, 0xbb, 0x7a, 0x56, 0x63, 0x9e, 0x17, 0x7d, 0x30, 0x1b, 0xeb ]);

    const pubKey = await schnorr.computePublicKey(pk);
    const msg = new TextEncoder().encode('The quick brown dog jumped over the lazy fox.');
    const signature = await schnorr.constructSignature(msg, pk);
    const verified = await schnorr.verifySignature(msg, pubKey, signature);

    expect(verified).toBe(true);
  });
});
