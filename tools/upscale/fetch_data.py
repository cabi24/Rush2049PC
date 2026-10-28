#!/usr/bin/env python3
"""Download the upscaler's training and test images: CC0 photo textures only (docs/port/UPSCALE.md).

    fetch_data.py --out DIR                      the files listed in sources.json, checked against its SHA-256s
    fetch_data.py --out DIR --discover           build the list again from the libraries' APIs, then fetch it and
                                                 write sources.json (what was committed is the list that was used)

Sources (both CC0 1.0, public domain dedication):
  Poly Haven  https://polyhaven.com/license      every texture's 1k diffuse JPEG (api.polyhaven.com)
  ambientCG   https://docs.ambientcg.com/license/ every 7th material's 1K-JPG colour map, and the leaf, foliage and
                                                 graffiti atlases' colour + opacity maps (ambientcg.com/api/v2)
Nothing else goes in: no game data, no other libraries. Each asset's split (train or test) comes from a hash of its
name, so it doesn't depend on the order of the list.
"""
import argparse
import concurrent.futures
import hashlib
import io
import json
import os
import sys
import time
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, "sources.json")
UA = {"User-Agent": "Rush2049PC-upscale-fetch/1 (training data for a texture upscaler)"}
AMBIENTCG_MATERIAL_STEP = 7
ATLAS_PREFIXES = ("LeafSet", "Foliage", "GraffitiSet")


def get(url, tries=4):
    for i in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
                return r.read()
        except Exception as e:   # network hiccups: retry, then give up loudly
            if i == tries - 1:
                raise RuntimeError(f"{url}: {e}")
            time.sleep(2 * (i + 1))


def split_of(asset):
    """About one asset in ten is held out for testing, chosen by name."""
    return "test" if int(hashlib.sha1(asset.encode()).hexdigest()[:8], 16) % 10 == 0 else "train"


def discover():
    entries = []
    ph = json.loads(get("https://api.polyhaven.com/assets?t=textures"))
    for asset in sorted(ph):
        entries.append({"source": "polyhaven", "asset": asset, "license": "CC0-1.0",
                        "authors": sorted(ph[asset].get("authors", {})), "split": split_of(asset)})

    def acg(kind):
        out, offset = [], 0
        while True:
            d = json.loads(get(f"https://ambientcg.com/api/v2/full_json?type={kind}&include=downloadData"
                               f"&limit=250&offset={offset}&sort=Alphabet"))
            out += d["foundAssets"]
            offset += 250
            if offset >= d["numberOfResults"]:
                return out

    mats = sorted(acg("Material"), key=lambda a: a["assetId"])
    atlases = sorted((a for a in acg("Atlas") if a["assetId"].startswith(ATLAS_PREFIXES)), key=lambda a: a["assetId"])
    for kind, assets in (("material", mats[::AMBIENTCG_MATERIAL_STEP]), ("atlas", atlases)):
        for a in assets:
            try:
                zips = a["downloadFolders"]["default"]["downloadFiletypeCategories"]["zip"]["downloads"]
            except (KeyError, TypeError):   # assets without the usual zip downloads
                continue
            z = [d for d in zips if d.get("attribute") == "1K-JPG"]
            if not z:
                continue
            entries.append({"source": "ambientcg", "kind": kind, "asset": a["assetId"], "license": "CC0-1.0",
                            "url": z[0]["fullDownloadPath"], "split": split_of(a["assetId"])})
    return entries


def fetch_one(e, out, check):
    """Download one asset; returns the entry with its files and checksums."""
    if e["source"] == "polyhaven":
        if "url" not in e:
            files = json.loads(get(f"https://api.polyhaven.com/files/{e['asset']}"))
            jpg = files.get("Diffuse", {}).get("1k", {}).get("jpg")
            if not jpg:
                return None
            e["url"] = jpg["url"]
        name = f"polyhaven/{e['asset']}.jpg"
        path = os.path.join(out, name)
        if not os.path.exists(path) or (check and hashlib.sha256(open(path, "rb").read()).hexdigest() != e["sha256"]):
            data = get(e["url"])
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)
        digest = hashlib.sha256(open(path, "rb").read()).hexdigest()
        if check and digest != e["sha256"]:
            raise RuntimeError(f"{e['url']}: SHA-256 {digest}, the manifest says {e['sha256']}")
        e["sha256"] = digest
        e["files"] = {"color": name}
        return e
    # ambientCG: the zip's SHA-256 is recorded; only its colour (and opacity) maps are kept.
    want = {"color": "_Color.jpg", "opacity": "_Opacity.jpg"}
    names = {k: f"ambientcg/{e['asset']}{v}" for k, v in want.items()}
    have = all(os.path.exists(os.path.join(out, names[k])) for k in ("color",))
    if have and not check and "sha256" in e:
        e["files"] = {k: n for k, n in names.items() if os.path.exists(os.path.join(out, n))}
        return e
    data = get(e["url"])
    digest = hashlib.sha256(data).hexdigest()
    if check and digest != e["sha256"]:
        raise RuntimeError(f"{e['url']}: SHA-256 {digest}, the manifest says {e['sha256']}")
    e["sha256"] = digest
    e["files"] = {}
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for member in z.namelist():
            for k, suffix in want.items():
                if member.endswith(suffix) and "/" not in member:
                    path = os.path.join(out, names[k])
                    os.makedirs(os.path.dirname(path), exist_ok=True)
                    with open(path, "wb") as f:
                        f.write(z.read(member))
                    e["files"][k] = names[k]
    return e if "color" in e["files"] else None


def write_manifest(path, assets):
    """One asset per line, so a change of list reads as a diff."""
    with open(path, "w") as f:
        f.write('{"comment": "CC0 images used to train and test port/upscale (tools/upscale/fetch_data.py)",\n')
        f.write(' "assets": [\n')
        f.write(",\n".join("  " + json.dumps(e, sort_keys=True) for e in assets))
        f.write("\n]}\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True, help="download directory (outside the repository)")
    ap.add_argument("--discover", action="store_true", help="rebuild the list from the APIs and rewrite sources.json")
    ap.add_argument("--manifest", default=MANIFEST)
    ap.add_argument("--jobs", type=int, default=8)
    args = ap.parse_args()
    if args.discover:
        entries, check = discover(), False
    else:
        entries, check = json.load(open(args.manifest))["assets"], True
    os.makedirs(args.out, exist_ok=True)
    done, failed = [], 0
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        futs = {pool.submit(fetch_one, dict(e), args.out, check): e for e in entries}
        for i, f in enumerate(concurrent.futures.as_completed(futs)):
            try:
                r = f.result()
            except Exception as ex:
                print(f"failed: {ex}", file=sys.stderr)
                failed += 1
                continue
            if r:
                done.append(r)
            if i % 100 == 0:
                print(f"{i}/{len(entries)}", file=sys.stderr)
    done.sort(key=lambda e: (e["source"], e["asset"]))
    with open(os.path.join(args.out, "index.json"), "w") as f:
        json.dump(done, f, indent=0)
    if args.discover:
        write_manifest(args.manifest, done)
    n = {s: sum(e["split"] == s for e in done) for s in ("train", "test")}
    print(f"{len(done)} assets ({n['train']} train, {n['test']} test), {failed} failed", file=sys.stderr)
    return 1 if failed and check else 0


if __name__ == "__main__":
    sys.exit(main())
