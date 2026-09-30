"""AUTHORED ONLY: consumes future real source_screen_codec_export captures.

Mutations below are rejection tests, never exported evidence or fabricated
game state. The legacy positive is an explicitly labelled adapter fixture made
from a live capture; it is not a claim to have received historical UI evidence.
"""
import argparse
import dataclasses
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from normal_session_native_readback import Compare, Observation, observations, u32
from normal_session_owner_codec import parse, read_wire
from normal_session_ui_readback import UiCompare


def require(value, message):
    if not value:
        raise AssertionError(message)


def compare(snapshot, obs, mode='nullable'):
    base = Compare(snapshot, {}, Path('.'))
    ui = UiCompare(base, obs, screen_mode=mode)
    result = ui.execute_screen()
    complete = base.finalize()
    require(complete['represented_bytes_compared'] == 69640, 'full screen wire coverage')
    require(not result['normal_parent_qualification_ready'], 'subset promoted to full parent')
    return result


def observation_change(obs, key, *, data=None, address=None):
    out = dict(obs)
    value = out[key]
    out[key] = Observation(value.address if address is None else address,
                           value.data if data is None else data)
    return out


def owner_change(snapshot, name, **changes):
    owners = tuple(dataclasses.replace(a, **changes) if a.name == name else a
                   for a in snapshot.allocations)
    return dataclasses.replace(snapshot, allocations=owners)


def run(capture):
    cases = {}
    payload = None
    for count in (0, 1, 255, 256, 0xffffffff):
        for state in ('null', 'live'):
            name = f'{count}-{state}'
            before = (capture / (name + '-before.bin')).read_bytes()
            require(before == (capture / (name + '-after.bin')).read_bytes(), 'export mutated actual world observations')
            obs = observations(before)
            wire = parse(read_wire(capture / (name + '-owners.bin')))
            require(len(wire) == 1, 'screen capture snapshot count')
            snapshot = wire[0]
            result = compare(snapshot, obs)
            require(result['ui_screen_count'] == count and result['ui_screen_null'] == (state == 'null'), 'raw count/publication mismatch')
            if payload is None:
                payload = obs['screen-records'].data
            require(obs['screen-records'].data == payload, 'publication/count changed real full payload')
            cases[name] = snapshot, obs
    null, no = cases['0-null']
    live, lo = cases['0-live']
    negatives = []

    def reject(label, call):
        try:
            call()
        except (AssertionError, ValueError, KeyError, TypeError, RuntimeError):
            negatives.append(label)
        else:
            raise AssertionError('accepted negative: ' + label)

    for missing in ('screen-base-null', 'screen-base-native-address'):
        bad = dict(lo)
        del bad[missing]
        reject('missing/' + missing, lambda bad=bad: compare(live, bad))
    legacy = {k: v for k, v in lo.items() if k not in ('screen-base-null', 'screen-base-native-address')}
    reject('missing-both-is-not-default-live', lambda: compare(live, legacy))
    legacy_result = compare(live, legacy, 'legacy-fixed-base')
    require(legacy_result['ui_screen_mode'] == 'legacy-fixed-base', 'explicit legacy adapter lost mode')
    reject('legacy-rejects-new-metadata', lambda: compare(live, lo, 'legacy-fixed-base'))
    partial = dict(legacy, **{'screen-base-null': lo['screen-base-null']})
    reject('legacy-rejects-partial-metadata', lambda: compare(live, partial, 'legacy-fixed-base'))
    reject('unknown-mode', lambda: compare(live, lo, 'automatic'))
    reject('null-flag-domain', lambda: compare(live, observation_change(lo, 'screen-base-null', data=struct.pack('<I', 2))))
    reject('null-flag-width', lambda: compare(live, observation_change(lo, 'screen-base-null', data=b'\x00')))
    reject('native-pointer-width', lambda: compare(live, observation_change(lo, 'screen-base-native-address', data=b'\0'*4)))
    reject('metadata-native-domain', lambda: compare(live, observation_change(lo, 'screen-base-null', address=123)))
    reject('live-pointer-zero', lambda: compare(live, observation_change(lo, 'screen-base-native-address', data=b'\0'*8)))
    reject('live-pointer-interior', lambda: compare(live, observation_change(lo, 'screen-base-native-address', data=struct.pack('<Q', lo['screen-records'].address+4))))
    reject('NULL-pointer-nonzero', lambda: compare(null, observation_change(no, 'screen-base-native-address', data=lo['screen-base-native-address'].data)))
    reject('count-native-overlap', lambda: compare(live, observation_change(lo, 'screen-count', address=lo['screen-records'].address)))
    reject('NULL-invented-root', lambda: compare(owner_change(null, 'ui-screen-root', data=struct.pack('<I', 0x72140000)), no))
    reject('live-zero-root', lambda: compare(owner_change(live, 'ui-screen-root', data=b'\0'*4), lo))
    root = next(a for a in live.allocations if a.name == 'ui-screen-root')
    reject('root-origin', lambda: compare(owner_change(live, 'ui-screen-root', claims=(dataclasses.replace(root.claims[0], origin=2),)), lo))
    reject('raw-count-mismatch', lambda: compare(owner_change(live, 'ui-screen-count', data=struct.pack('<I', 256)), lo))
    changed = bytearray(payload)
    changed[-1] ^= 1
    reject('retained-tail-mismatch', lambda: compare(owner_change(live, 'ui-screen-records', data=bytes(changed)), lo))
    reject('retained-tail-missing', lambda: compare(owner_change(live, 'ui-screen-records', data=payload[:-4]), lo))
    reject('live-alias-missing', lambda: compare(dataclasses.replace(live, aliases=()), lo))
    reject('NULL-alias-invented', lambda: compare(dataclasses.replace(null, aliases=live.aliases), no))
    alias = live.aliases[0]
    reject('live-alias-one-past', lambda: compare(dataclasses.replace(live, aliases=(dataclasses.replace(alias, one_past=True),)), lo))
    require(len(negatives) == 23, 'missing authored negative case')
    return {'passed': True, 'actual_screen_captures': len(cases),
            'negative_cases': negatives, 'legacy_adapter_fixtures': 1,
            'historical_UI_captures_received': 0, 'full_payload_bytes_per_capture': len(payload),
            'normal_parent_qualification_ready': False, 'source_executions': 0,
            'native_executions': 0}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    print(json.dumps(run(args.capture), indent=2))
