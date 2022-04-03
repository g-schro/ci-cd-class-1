import argparse
import junit_xml as jux
import logging
import pexpect as pex
import pexpect.popen_spawn as pos
import psutil
import signal
import subprocess
import sys
import time

_log = logging.getLogger()
_log_handler = logging.StreamHandler(sys.stdout)
_log.addHandler(_log_handler)
_log.setLevel(logging.ERROR)

g_dut = None
g_sim = None
g_test_name = None
g_prompt = '> '
g_test_unit = None
g_test_junits = []

class GpioDev:
    """ This class represents a device that support the GPIO CLI. """

    def __init__(self, name, serial_dev, baud_rate=115200):
        self.name = name
        _log.debug('[%s] Connecting to %s at %d', self.name, serial_dev,
                   baud_rate)
        self.console = pos.PopenSpawn('plink -serial %s -sercfg %d' %
                                      (serial_dev, baud_rate))

    def set_timeout(self, timeout):
        self.console.timeout = timeout

    def flush_input(self):
        _log.debug('[%s] flush_input()', self.name)
        while self.console.expect([pex.TIMEOUT, pex.EOF, '.*']) == 2:
            if len(self.console.after) == 0:
                break
            _log.debug('[%s] In flush_input() got %s', self.name,
                       self.console.after)

    def send_line(self, msg):
        _log.debug('[%s] Sending "%s"', self.name, msg)
        self.console.sendline(msg)

    def get_pattern_list(self, pattern_list):
        """ Wait for a series of patterns from the device, with an arbitrary amount
        of text allowed between the patterns.

        pattern_list - a list of text strings (can be regex) to wait for, in order.
        """

        rc = 0
        for pat in pattern_list:
            rc = self.console.expect([pat, pex.TIMEOUT, pex.EOF])
            if rc != 0:
                _log.debug('[%s] In get_pattern_list() failure for "%s" (rc=%d) for test "%s"',
                           self.name, pat, rc, g_test_name)
                return rc, pat
            _log.debug('[%s] Expecting "%s" got "%s%s"', self.name, pat,
                       self.console.before, self.console.after)
        return rc, None

    def get_prompt(self):
        rc, failed_pat = self.get_pattern_list([g_prompt])
        if rc != 0:
            _log.debug('[%s] In get_prompt() failure rc=%d', self.name, rc)
        return rc

    def do_reset(self):
        self.flush_input()
        self.send_line('reset')
        rc, failed_pat = self.get_pattern_list(['Starting', g_prompt])
        if rc != 0:
            _log.debug('[%s] In do_prompt() failure rc=%d pat=%s', self.name,
                       rc, failed_pat)
        return rc, failed_pat

    def terminate(self):
        if self.console is not None:
            _log.debug('[%s] Killing', self.name)

            # It seems we need to use SIGTERM, and not CTRL_C_EVENT, to be
            # full effective. It gives an "access denied" exception, but still
            # seems to work.

            try:
                self.console.kill(signal.SIGTERM)
            except Exception as e:
                _log.debug('[%s] Exception on kill: %s', self.name, str(e))

def testing_init(dut_serial, sim_serial):
    """ Initialize testing software. """

    global g_dut
    global g_sim

    kill_procs = []
    for p in psutil.process_iter():
        if p.name() == 'plink.exe':
            for serial in (dut_serial, sim_serial):
                if serial.upper() in [s.upper() for s in p.cmdline()]:
                    kill_procs.append(p)

    for p in kill_procs:
        _log.debug('Killing process %s %s (pid=%d)', p.name(), p.cmdline(),
                   p.pid)
        p.kill()

    g_dut = GpioDev('dut', dut_serial)
    if sim_serial.upper() != 'NONE':
        g_sim = GpioDev('sim', sim_serial)

################################################################################
def start_test(name, timeout=3):
    """ Start a test, including recording the test name and junit object.

    name - the name of the test
    timeout - how long to wait for results.
    """

    global g_test_name
    global g_test_junit

    g_test_junit = jux.TestCase(name)
    g_test_junits.append(g_test_junit)
    g_test_name = name
    _log.debug('Test "%s" starting', g_test_name)
    for dev in (g_dut, g_sim):
        if dev is not None:
            dev.set_timeout(timeout)
            dev.flush_input()

################################################################################
def test_fail(failure_info):
    """ Handle a test failure, recording info and restarting the board.

    failure_info - string describing how it failed.
    """

    _log.info('Test "%s" fails: %s', g_test_name, failure_info)
    g_test_junit.add_failure_info(failure_info)

    # After a test fail, try to reset the MCU to clean things up for the next
    # test. No guarantee it works.
    for dev in (g_dut, g_sim):
        if dev is not None:
            dev.do_reset()

################################################################################
def test_pass():
    """ Handle a test pass, recording info. """

    _log.info('Test "%s" passes', g_test_name)
    for dev in (g_dut, g_sim):
        if dev is not None:
            dev.flush_input()

################################################################################
# Test: console_prompt
################################################################################
def test_console_prompt():
    """ Send a carriage return and wait for the console prompt. """

    passed = True
    start_test('console_prompt')
    g_dut.send_line('')
    rc = g_dut.get_prompt()
    if rc != 0:
        test_fail('Did not receive prompt rc=%d' % rc)
        passed = False
    else:
        test_pass()
    return passed

################################################################################
# Test: test_version
################################################################################
def test_version(tver):
    """ Test to verify the software version is as expected.

    tver - the software version string.
    """

    passed = True
    start_test('version')
    g_dut.send_line('version')
    pat_list = ['Version="' + tver + '"', g_prompt]
    rc, failed_pat = g_dut.get_pattern_list(pat_list)
    if rc != 0:
        test_fail('Did not find pattern "%s" rc=%d' % (failed_pat, rc))
        passed = False
    else:
        test_pass()
    return passed

################################################################################
# Test: reset
################################################################################
def test_reset():
    """ Test to verify the reset command works. """

    passed = True
    start_test('reset')
    rc, failed_pt = g_dut.do_reset()
    if rc != 0:
        test_fail('Did not find pattern "%s" rc=%d' % (failed_pat, rc))
        passed = False
    else:
        test_pass()
    return passed

################################################################################
# Test: help
################################################################################
def test_help():
    """ Test to verify the console help command works. """

    passed = True
    start_test('help')
    g_dut.send_line('help')
    pat_list = ['dc ', 'dr ', 'dw ', 'reset', 'version', g_prompt]
    rc, failed_pat = g_dut.get_pattern_list(pat_list)
    if rc != 0:
        test_fail('Did not find pattern "%s" rc=%d' % (failed_pat, rc))
        passed = False
    else:
        test_pass()
    return passed

################################################################################
# Test: gpio
################################################################################

def test_gpio():

    # Tests with names beginning with D use both boards, while tests with names
    # starting with S use a single board (sim hardware not used).

    gpio_tests = {

        # Test D-1: DUT with pushpull output, SIM with no-pull input.
        'd-1' : [
            [g_dut,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dc b 9 i nopull',   'OK'],
            [g_dut,    'dw b 9 1',          'OK'],
            [g_sim,    'dr b 9',            '"1"'],
            [g_dut,    'dw b 9 0',          'OK'],
            [g_sim,    'dr b 9',            '"0"'],
            ],
        # Test D-2: DUT with pushpull output, SIM with pullup input.
        'd-2' : [
            [g_dut,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dc b 9 i up',       'OK'],
            [g_dut,    'dw b 9 1',          'OK'],
            [g_sim,    'dr b 9',            '"1"'],
            [g_dut,    'dw b 9 0',          'OK'],
            [g_sim,    'dr b 9',            '"0"'],
            ],
        # Test D-3: DUT with pushpull output, SIM with pulldown input.
        'd-3' : [
            [g_dut,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dc b 9 i down',     'OK'],
            [g_dut,    'dw b 9 1',          'OK'],
            [g_sim,    'dr b 9',            '"1"'],
            [g_dut,    'dw b 9 0',          'OK'],
            [g_sim,    'dr b 9',            '"0"'],
            ],
        # Test D-4: DUT with opendrain output, SIM with pullup input.
        'd-4' : [
            [g_dut,    'dc b 9 o nopull',   'OK'],
            [g_sim,    'dc b 9 i up',       'OK'],
            [g_dut,    'dw b 9 1',          'OK'],
            [g_sim,    'dr b 9',             '"1"'],
            [g_dut,    'dw b 9 0',          'OK'],
            [g_sim,    'dr b 9',             '"0"'],
            ],
        # Test D-5: DUT with opendrain output, SIM with pulldown input.
        'd-5' : [
            [g_dut,    'dc b 9 o nopull',   'OK'],
            [g_sim,    'dc b 9 i down',     'OK'],
            [g_dut,    'dw b 9 1',          'OK'],
            [g_sim,    'dr b 9',            '"0"'],
            [g_dut,    'dw b 9 0',          'OK'],
            [g_sim,    'dr b 9',            '"0"'],
            ],
        # Test D-6: DUT with nopull input, SIM with pushpull output.
        'd-6' : [
            [g_dut,    'dc b 9 i nopull',   'OK'],
            [g_sim,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dw b 9 1',          'OK'],
            [g_dut,    'dr b 9',            '"1"'],
            [g_sim,    'dw b 9 0',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            ],
        # Test D-7: DUT with pulldown input, SIM with pushpull output.
        'd-7' : [
            [g_dut,    'dc b 9 i down',     'OK'],
            [g_sim,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dw b 9 1',          'OK'],
            [g_dut,    'dr b 9',            '"1"'],
            [g_sim,    'dw b 9 0',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            ],
        # Test D-8: DUT with pullup input, SIM with pushpull output.
        'd-8' : [
            [g_dut,    'dc b 9 i up',       'OK'],
            [g_sim,    'dc b 9 o pushpull', 'OK'],
            [g_sim,    'dw b 9 1',          'OK'],
            [g_dut,    'dr b 9',            '"1"'],
            [g_sim,    'dw b 9 0',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            ],
        # Test D-9: DUT with pullup input, SIM with opendrain output.
        'd-9' : [
            [g_dut,    'dc b 9 i up',       'OK'],
            [g_sim,    'dc b 9 o nopull',   'OK'],
            [g_sim,    'dw b 9 1',          'OK'],
            [g_dut,    'dr b 9',            '"1"'],
            [g_sim,    'dw b 9 0',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            ],
        # Test D-10: DUT with pulldown input, SIM with opendrain output.
        'd-10' : [
            [g_dut,    'dc b 9 i down',     'OK'],
            [g_sim,    'dc b 9 o nopull',   'OK'],
            [g_sim,    'dw b 9 1',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            [g_sim,    'dw b 9 0',          'OK'],
            [g_dut,    'dr b 9',            '"0"'],
            ],

        # Test S-1: DUT PC5 with pushpull output, DUT PC6 with no-pull input.
        's-1' : [
            [g_dut,    'dc c 5 o pushpull', 'OK'],
            [g_dut,    'dc c 6 i nopull',   'OK'],
            [g_dut,    'dw c 5 1',          'OK'],
            [g_dut,    'dr c 6',            '"1"'],
            [g_dut,    'dw c 5 0',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            ],
        # Test S-2: DUT PC5 with pushpull output, DUT PC6 with pullup input.
        's-2' : [
            [g_dut,    'dc c 5 o pushpull', 'OK'],
            [g_dut,    'dc c 6 i up',       'OK'],
            [g_dut,    'dw c 5 1',          'OK'],
            [g_dut,    'dr c 6',            '"1"'],
            [g_dut,    'dw c 5 0',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            ],
        # Test S-3: DUT PC5 with pushpull output, DUT PC6 with pulldown input.
        's-3' : [
            [g_dut,    'dc c 5 o pushpull', 'OK'],
            [g_dut,    'dc c 6 i down',     'OK'],
            [g_dut,    'dw c 5 1',          'OK'],
            [g_dut,    'dr c 6',            '"1"'],
            [g_dut,    'dw c 5 0',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            ],
        # Test S-4: DUT PC5 with opendrain output, DUT PC6 with pullup input.
        's-4' : [
            [g_dut,    'dc c 5 o nopull',   'OK'],
            [g_dut,    'dc c 6 i up',       'OK'],
            [g_dut,    'dw c 5 1',          'OK'],
            [g_dut,    'dr c 6',            '"1"'],
            [g_dut,    'dw c 5 0',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            ],
        # Test S-5: DUT PC5 with opendrain output, DUT PC6 with pulldown input.
        's-5' : [
            [g_dut,    'dc c 5 o nopull',   'OK'],
            [g_dut,    'dc c 6 i down',     'OK'],
            [g_dut,    'dw c 5 1',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            [g_dut,    'dw c 5 0',          'OK'],
            [g_dut,    'dr c 6',            '"0"'],
            ],
        }

    for name, test_steps in gpio_tests.items():
        # Check if test uses sim.
        skip_test = False
        for test_step in test_steps:
            if test_step[0] is None:
                skip_test = True
                break
        if skip_test:
            continue
        test_name = 'gpio-%s' % name
        start_test(test_name)
        step_idx = -1
        rc = 0
        for test_step in test_steps:
            step_idx += 1
            dev, send_msg, expect_msg = test_step
            dev.send_line(send_msg)
            rc, failed_pat = dev.get_pattern_list([expect_msg, g_prompt])
            if rc != 0:
                test_fail('At step idx %d did not find pattern "%s" rc=%d' %
                          (step_idx, failed_pat, rc))
                break
        if rc == 0:
            test_pass()

################################################################################
def main():
    _log.setLevel(logging.INFO)

    parser = argparse.ArgumentParser()
    parser.add_argument('--log',
                        help='notset|debug|info|warning|error|critical (default warning)',
                        default='warning')
    parser.add_argument('--jfile',
                        help='filename of JUnit XML output file (default test-results.xml)',
                        default='test-results.xml')
    parser.add_argument('--tver',
                        help='Target software version ID (default None)',
                        default='None')
    parser.add_argument('--dut-serial',
                        help='Device under test console device (default COM4)',
                        default='COM4')
    parser.add_argument('--sim-serial',
                        help='Simulation hardware console device, set to none if not present (default COM3)',
                        default='COM3')
    args = parser.parse_args()

    log_map = {
        'CRITICAL' : logging.CRITICAL,
        'ERROR' : logging.ERROR,
        'WARNING' : logging.WARNING,
        'INFO' : logging.INFO,
        'DEBUG' : logging.DEBUG,
        'NOTSET' : logging.NOTSET,
        }

    if args.log.upper() not in log_map:
        print('ERROR: Invalid log level. Valid levels are (case does not matter):')
        for level, int_level in log_map.items():
            print('  %s' % (level.lower()))
        exit(1)
    _log.setLevel(log_map[args.log.upper()])

    testing_init(args.dut_serial, args.sim_serial)

    test_console_prompt()
    test_version(args.tver)
    test_reset()
    test_help()
    test_gpio()

    test_suite = jux.TestSuite(args.jfile, g_test_junits)
    print(jux.TestSuite.to_xml_string([test_suite]))
    with open(args.jfile, 'w') as f:
        jux.TestSuite.to_file(f, [test_suite], prettyprint=True)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('ERROR: Got keyboard interrupt')
    finally:
        for dev in (g_dut, g_sim):
            if dev is not None:
                try:
                    dev.terminate()
                except Exception as e:
                    _log.error('[%s] Exception on terminate: %s', dev.name,
                               str(e))
