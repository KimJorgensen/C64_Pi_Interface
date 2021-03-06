ifeq "$(release)" "yes"
	version := 1.0
else
	version := $(shell date +%y%m%d-%H%M)
endif

here        := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
synth_dir   := $(here)/synth
src_dir     := $(here)/src
vhdlprj     := cpi

# this file list is for dependencies only, keep in sync with $(vhdlprj).prj
src         := $(src_dir)/$(vhdlprj).vhdl
src         += $(src_dir)/exp_bus_ctrl.vhdl

uc         := $(src_dir)/$(vhdlprj).ucf

xst_cfg    := $(here)/$(vhdlprj).xst
project    := $(here)/$(vhdlprj).prj

#netlist    := $(addprefix $(synth_dir)/, $(notdir $(src:.vhdl=.ngc)))
netlist    := $(synth_dir)/$(vhdlprj).ngc

gen_db     := $(synth_dir)/$(vhdlprj).ngd
fit        := $(synth_dir)/$(vhdlprj).vm6
jedec      := $(here)/$(vhdlprj).jed
svf        := $(here)/$(vhdlprj).svf

dev_type   := xc95144xl
dev_pkg    := tq100
dev_speed  := 10
device     := $(dev_type)-$(dev_pkg)-$(dev_speed)
device_fit := $(dev_type)-$(dev_speed)-$(dev_pkg)

fit_flags  := -p $(device_fit) -ofmt abel -log fitter.log -optimize density
fit_flags  += -power low -slew slow
#fit_flags  += -exhaust
fit_flags  += -inputs 54 -pterms 25
fit_filter_output := "^CS: block property:\|^$$"

# directories to be created
dirs       := $(synth_dir) $(here)/log

.PHONY: all
ifeq "$(release)" "yes"
all: $(vhdlprj)-cpld-$(version).zip
else
all: $(svf)
endif

################################################################################
$(vhdlprj)-cpld-$(version).zip: $(svf) README.txt
	rm -rf $(vhdlprj)-cpld-$(version)
	rm -f $@
	mkdir $(vhdlprj)-cpld-$(version)
	cp $(svf) $(vhdlprj)-cpld-$(version)/$(vhdlprj)-cpld-$(version).svf
	cp README.txt $(vhdlprj)-cpld-$(version)
	zip -v -r $@ $(vhdlprj)-cpld-$(version)

################################################################################
.PHONY: netlist
netlist: $(netlist)
$(netlist): $(xst_cfg) $(src) $(project) | $(dirs)
	mkdir -p $(synth_dir)/xst/tmp/
	xst -intstyle silent -ifn $<
	mv $(vhdlprj).srp log/netlist.$(vhdlprj).srp

################################################################################
.PHONY: gen_db
gen_db: $(gen_db)
$(gen_db): $(netlist) $(uc) | $(dirs)
	mkdir -p synth/ngdbuild/tmp/
	cd $(synth_dir) && ngdbuild -p $(device) -uc $(uc) -quiet \
		-intstyle silent -dd $(synth_dir)/ngdbuild/tmp/ $<
	mv $(synth_dir)/$(vhdlprj).bld log/ngd.$(vhdlprj).bld

################################################################################
.PHONY: fit
fit: $(fit)
$(fit): $(gen_db) | $(dirs)
	cd $(synth_dir) && cpldfit $(fit_flags) $< | grep -v $(fit_filter_output)
	mv $(synth_dir)/$(vhdlprj).rpt log/fitter.$(vhdlprj).rpt

################################################################################
.PHONY: jedec
jedec: $(jedec)
$(jedec): $(fit)
	hprep6 -i $<

################################################################################
.PHONY: svf
svf: $(svf)
$(svf): $(jedec)
	cat impact.batch | impact -batch
	mv _impactbatch.log log/

################################################################################
.PHONY: usbprog
usbprog: $(svf)
	svfplayer $<

################################################################################
.PHONY: prog
prog: $(svf)
	easp -p 0x8738 $<

################################################################################
$(dirs):
	mkdir -p $@

################################################################################
test:
	mkdir -p $(here)/bench
	export XILINX=/opt/Xilinx/14.7/ISE_DS/ && \
	bash /opt/Xilinx/14.7/ISE_DS/settings64.sh && \
	cd $(here)/bench && \
	fuse -intstyle ise -incremental -o test_bench -prj ../test_bench.prj test_bench && \
	./test_bench -tclbatch ../test_bench.cmd -gui

################################################################################
.PHONY: clean
clean:
	rm -rf synth
	rm -rf log
	rm -rf _xmsgs
	rm -rf bench
	rm -f $(vhdlprj).jed
	rm -f $(vhdlprj).svf
	rm -f tmperr.err
