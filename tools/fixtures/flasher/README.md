# Flasher regression assets

Fixed copies of the public flasher integrity script and serial adapter, plus a manifest whose bytes are replaced by synthetic data in the test. These are executable regression inputs, not firmware or a deployable website. Preserve their license notices. `node tools/flasher_integrity_host_test.cjs` checks wrong versions, corruption, stale caches, resource release and verification before opening a port, without `flash/` or network access.
