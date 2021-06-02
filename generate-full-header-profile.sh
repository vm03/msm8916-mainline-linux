
echo "# generating full header profiling warning additions"
sed --follow-symlinks -iE '0,/#define/s//#warning profile\n#define/' $(git ls-files 'arch/x86/include/**.h' 'include/**.h' | grep -v kallsyms_objtool.h )


