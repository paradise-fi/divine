GPP = g++

#DEFINES  =
#DEBUGFLAGS = -g -fno-default-inline -fkeep-inline-functions
OPTFLAGS = -O3 -fkeep-inline-functions

CFLAGS = -I. -Wall -DINLINE_BITSET $(DEFINES) $(DEBUGFLAGS) $(OPTFLAGS)

# uncomment the following line to compile the sample plugins
#PLUGINS = plugins/SamplePlugin.o plugins/DRAReachabilityPlugin.o plugins/WriteDRAPlugin.o

OBJS = $(PLUGINS) common/BitSet.o common/RunProgram.win32.o common/RunProgram.posix.o StutterSensitivenessInformation.o LTL2DRA.o LTL2DSTAR_Scheduler.o APMonom.o parsers/nba-parser-lbtt.lex.o  parsers/nba-parser-lbtt.tab.o parsers/nba-parser-promela.lex.o  parsers/nba-parser-promela.tab.o plugins/PluginManager.o ltl2dstar_main.o

all : ltl2dstar

ltl2dstar : $(OBJS) 
	$(GPP) -g $(LINK_FLAGS) -o ltl2dstar $+

%.o : %.cpp
	$(GPP) -c $(CFLAGS) -o $@ $<

clean :
	rm -f ltl2dstar *.o common/*.o parsers/*.o plugins/*.o
