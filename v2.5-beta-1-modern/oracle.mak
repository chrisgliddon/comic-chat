# oracle.mak - nmake file for the Comic Chat oracle harness.
#
# Builds OracleHarness.exe: a console subsystem app that links the real
# engine .obj files from v2.5-beta-1-modern + static MFC, with the
# ORACLE_HARNESS define active (enabling the determinism hooks).
#
# Must be run from the v2.5-beta-1-modern directory:
#   call "<VS>\VC\Auxiliary\Build\vcvars32.bat"
#   nmake /f oracle.mak CFG="oracle - Win32 Release"
#
# The engine objs are built by the normal chat.mak first (or this
# makefile builds them with the extra /D "ORACLE_HARNESS" flag).

!IF "$(CFG)" == ""
CFG=oracle - Win32 Release
!ENDIF

!IF "$(CFG)" == "oracle - Win32 Release"
OUTDIR=.\Release
INTDIR=.\Release
CPP_CFG=/MT /O2 /D "NDEBUG" /D "ORACLE_HARNESS"
RSC_CFG=/D "NDEBUG"
!ELSE
OUTDIR=.\Debug
INTDIR=.\Debug
CPP_CFG=/MTd /Od /D "_DEBUG" /D "ORACLE_HARNESS"
RSC_CFG=/D "_DEBUG"
!ENDIF

CPP=cl.exe
LINK32=link.exe

ARTINC=..\artifacts\inc
ARTLIB=..\artifacts\lib\i386
ORACLE=..\oracle\harness

# Engine source files needed for the harness. We compile these WITH
# ORACLE_HARNESS defined so the #ifdef'd seed hooks are active.
# We use a separate intdir to avoid clashing with chat.mak's objs.
ORACLE_INTDIR=$(INTDIR)\oracle

CPP_PROJ=/nologo $(CPP_CFG) /W3 /GX /Zi /Zc:forScope- /Zc:strictStrings- /D "WIN32" /D "_CONSOLE" /D "_MBCS" \
 /I "." /I "$(ARTINC)" /I "$(ORACLE)" /Fo"$(ORACLE_INTDIR)\\" /Fd"$(ORACLE_INTDIR)\\" /c

# Engine files (subset that the harness links -- the logic tier files)
ENGINE_OBJS= \
	"$(ORACLE_INTDIR)\stdafx.obj" \
	"$(ORACLE_INTDIR)\arc.obj" \
	"$(ORACLE_INTDIR)\avatar.obj" \
	"$(ORACLE_INTDIR)\avatario.obj" \
	"$(ORACLE_INTDIR)\avbfile.obj" \
	"$(ORACLE_INTDIR)\backdrop.obj" \
	"$(ORACLE_INTDIR)\balloon.obj" \
	"$(ORACLE_INTDIR)\bbox.obj" \
	"$(ORACLE_INTDIR)\chat.obj" \
	"$(ORACLE_INTDIR)\chatDoc.obj" \
	"$(ORACLE_INTDIR)\dib.obj" \
	"$(ORACLE_INTDIR)\fonts.obj" \
	"$(ORACLE_INTDIR)\format.obj" \
	"$(ORACLE_INTDIR)\histent.obj" \
	"$(ORACLE_INTDIR)\panel.obj" \
	"$(ORACLE_INTDIR)\splinutl.obj" \
	"$(ORACLE_INTDIR)\spline.obj" \
	"$(ORACLE_INTDIR)\traj.obj" \
	"$(ORACLE_INTDIR)\vector2d.obj" \
	"$(ORACLE_INTDIR)\utils.obj" \
	"$(ORACLE_INTDIR)\userinfo.obj" \
	"$(ORACLE_INTDIR)\ccommon.obj" \
	"$(ORACLE_INTDIR)\ccomp.obj" \
	"$(ORACLE_INTDIR)\chicdial.obj" \
	"$(ORACLE_INTDIR)\coolbar.obj" \
	"$(ORACLE_INTDIR)\intl.obj" \
	"$(ORACLE_INTDIR)\jis2sjis.obj" \
	"$(ORACLE_INTDIR)\sjis2jis.obj" \
	"$(ORACLE_INTDIR)\pageview.obj" \
	"$(ORACLE_INTDIR)\protsupp.obj" \
	"$(ORACLE_INTDIR)\ircsock.obj" \
	"$(ORACLE_INTDIR)\ircproto.obj" \
	"$(ORACLE_INTDIR)\chatbars.obj" \
	"$(ORACLE_INTDIR)\binddoc.obj" \
	"$(ORACLE_INTDIR)\binddcmt.obj" \
	"$(ORACLE_INTDIR)\binditem.obj" \
	"$(ORACLE_INTDIR)\bindtarg.obj" \
	"$(ORACLE_INTDIR)\bindview.obj" \
	"$(ORACLE_INTDIR)\bindauto.obj" \
	"$(ORACLE_INTDIR)\bindipfw.obj" \
	"$(ORACLE_INTDIR)\mfcbind.obj" \
	"$(ORACLE_INTDIR)\oleobjct.obj" \
	"$(ORACLE_INTDIR)\chatItem.obj" \
	"$(ORACLE_INTDIR)\chatsrv.obj" \
	"$(ORACLE_INTDIR)\doskey.obj" \
	"$(ORACLE_INTDIR)\spltchat.obj" \
	"$(ORACLE_INTDIR)\childfrm.obj" \
	"$(ORACLE_INTDIR)\IpFrame.obj" \
	"$(ORACLE_INTDIR)\MainFrm.obj" \
	"$(ORACLE_INTDIR)\chatView.obj" \
	"$(ORACLE_INTDIR)\textview.obj" \
	"$(ORACLE_INTDIR)\textcore.obj" \
	"$(ORACLE_INTDIR)\textpose.obj" \
	"$(ORACLE_INTDIR)\query.obj" \
	"$(ORACLE_INTDIR)\actions.obj" \
	"$(ORACLE_INTDIR)\admindlg.obj" \
	"$(ORACLE_INTDIR)\autopage.obj" \
	"$(ORACLE_INTDIR)\bodycam.obj" \
	"$(ORACLE_INTDIR)\chanprop.obj" \
	"$(ORACLE_INTDIR)\colordlg.obj" \
	"$(ORACLE_INTDIR)\filesend.obj" \
	"$(ORACLE_INTDIR)\memblst.obj" \
	"$(ORACLE_INTDIR)\motd.obj" \
	"$(ORACLE_INTDIR)\notif.obj" \
	"$(ORACLE_INTDIR)\notipage.obj" \
	"$(ORACLE_INTDIR)\print.obj" \
	"$(ORACLE_INTDIR)\proppage.obj" \
	"$(ORACLE_INTDIR)\RoomList.obj" \
	"$(ORACLE_INTDIR)\rtfcmb.obj" \
	"$(ORACLE_INTDIR)\rtfctrl.obj" \
	"$(ORACLE_INTDIR)\rules.obj" \
	"$(ORACLE_INTDIR)\saywnd.obj" \
	"$(ORACLE_INTDIR)\setupdlg.obj" \
	"$(ORACLE_INTDIR)\sounddlg.obj" \
	"$(ORACLE_INTDIR)\status.obj" \
	"$(ORACLE_INTDIR)\tabbar.obj" \
	"$(ORACLE_INTDIR)\txtfntdg.obj" \
	"$(ORACLE_INTDIR)\urlutil.obj" \
	"$(ORACLE_INTDIR)\userlist.obj" \
	"$(ORACLE_INTDIR)\webreq.obj" \
	"$(ORACLE_INTDIR)\whisprbx.obj" \
	"$(ORACLE_INTDIR)\mcithrd.obj" \
	"$(ORACLE_INTDIR)\dlylddll.obj"

# Oracle harness specific objs
HARNESS_OBJS= \
	"$(ORACLE_INTDIR)\oracleharness.obj" \
	"$(ORACLE_INTDIR)\ojson.obj" \
	"$(ORACLE_INTDIR)\oracleseed.obj"

ALL : "$(OUTDIR)\OracleHarness.exe"

"$(ORACLE_INTDIR)" :
	if not exist "$(ORACLE_INTDIR)/$(NULL)" mkdir "$(ORACLE_INTDIR)"

# ---- Link (console subsystem) ----
# NOTE: chat.res is linked so that CString::LoadString can read the emotion
# rule strings (ID_RULE_*) from the resource segment. Without it, the rules
# load as empty strings and GetEmotionsFromString returns no emotions — which
# the corpus didn't catch (no corpus case exercises a non-zero emotion dump).
# The --textpose Tier-1 #1 dump mode exposed this gap.
"$(OUTDIR)\OracleHarness.exe" : "$(ORACLE_INTDIR)" icchat.h $(ENGINE_OBJS) $(HARNESS_OBJS) "$(ORACLE_INTDIR)\chat.res"
	$(LINK32) /nologo /subsystem:console /FORCE:MULTIPLE /incremental:no /debug \
 /machine:I386 /nodefaultlib:"libc" \
 /LIBPATH:"$(VCTOOLSINSTALLDIR)ATLMFC\lib\spectre\x86" /LIBPATH:"$(ARTLIB)" \
 $(ENGINE_OBJS) $(HARNESS_OBJS) "$(ORACLE_INTDIR)\chat.res" \
 uuid.lib secur32.lib comctl32.lib ole32.lib oleaut32.lib oldnames.lib wsock32.lib \
 shell32.lib winmm.lib imm32.lib winspool.lib comdlg32.lib oledlg.lib wininet.lib zlib.lib \
 /out:"$(OUTDIR)\OracleHarness.exe"

# ---- Resource (chat.rc -> chat.res) ----
# Compile the resource so LoadString(ID_RULE_*) works in the harness.
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(ORACLE_INTDIR)\chat.res" /i "." /i "$(ARTINC)" $(RSC_CFG)
"$(ORACLE_INTDIR)\chat.res" : chat.rc
	$(RSC) $(RSC_PROJ) chat.rc

# ---- COM proxy from IDL ----
icchat_i.c icchat.h : base\icchat.idl
	midl.exe /nologo /I "$(ARTINC)" /h icchat.h /iid icchat_i.c base\icchat.idl

"$(ORACLE_INTDIR)\icchat_i.obj" : icchat_i.c icchat.h
	$(CPP) $(CPP_PROJ) icchat_i.c

# ---- Oracle harness sources ----
"$(ORACLE_INTDIR)\oracleharness.obj" : "$(ORACLE)\oracleharness.cpp"
	$(CPP) $(CPP_PROJ) "$(ORACLE)\oracleharness.cpp"

"$(ORACLE_INTDIR)\ojson.obj" : "$(ORACLE)\ojson.cpp"
	$(CPP) $(CPP_PROJ) "$(ORACLE)\ojson.cpp"

"$(ORACLE_INTDIR)\oracleseed.obj" : "$(ORACLE)\oracleseed.cpp"
	$(CPP) $(CPP_PROJ) "$(ORACLE)\oracleseed.cpp"

# ---- Engine sources (compiled with ORACLE_HARNESS) ----
# stdafx.cpp first (no PCH, but compiles the common header)
"$(ORACLE_INTDIR)\stdafx.obj" : stdafx.cpp
	$(CPP) $(CPP_PROJ) stdafx.cpp

# Rule for .c files
{.}.c{$(ORACLE_INTDIR)}.obj:
	$(CPP) $(CPP_PROJ) $<

# Rule for .cpp files -- we need to handle the explicit deps above and
# this catch-all. The explicit rules take precedence.
# Note: nmake processes inference rules only if no explicit rule matches.
{.}.cpp{$(ORACLE_INTDIR)}.obj:
	$(CPP) $(CPP_PROJ) $<

# ---- Clean ----
clean :
	if exist "$(ORACLE_INTDIR)" rmdir /s /q "$(ORACLE_INTDIR)"
	if exist "$(OUTDIR)\OracleHarness.exe" del /q "$(OUTDIR)\OracleHarness.exe"
	if exist "$(OUTDIR)\OracleHarness.pdb" del /q "$(OUTDIR)\OracleHarness.pdb"