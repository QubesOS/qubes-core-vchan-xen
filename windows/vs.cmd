for %%p in ("%cd%\..\..") do set QUBES_REPO=%%~fp\artifacts
set QB_LOCAL=1
set QB_SCRIPTS=%QUBES_BUILDER%\qubesbuilder\plugins\build_windows\scripts
start vs2022\core-vchan-xen.sln
