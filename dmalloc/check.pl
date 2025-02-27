#! /usr/bin/perl -w
use Time::HiRes;
use POSIX;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);

my($Red, $Redctx, $Green, $Greenctx, $Cyan, $Off) = ("\x1b[01;31m", "\x1b[0;31m", "\x1b[01;32m", "\x1b[0;32m", "\x1b[01;36m", "\x1b[0m");
$Red = $Redctx = $Green = $Greenctx = $Cyan = $Off = "" if !-t STDERR || !-t STDOUT;
my($ContextLines, $LeakCheck, $Make, $Test, $Exec) = (1, 0, 0, 0, 0);
my(@Restrict, @Makeargs);

$SIG{"CHLD"} = sub {};
my($run61_pid);

sub run_sh61_pipe ($$;$) {
    my($text, $fd, $size) = @_;
    my($n, $buf) = (0, "");
    return $text if !defined($fd);
    while ((!defined($size) || length($text) <= $size)
           && defined(($n = POSIX::read($fd, $buf, 8192)))
           && $n > 0) {
        $text .= substr($buf, 0, $n);
    }
    return $text;
}

sub run_sh61 ($;%) {
    my($command, %opt) = @_;
    my($outfile) = exists($opt{"stdout"}) ? $opt{"stdout"} : undef;
    my($size_limit_file) = exists($opt{"size_limit_file"}) ? $opt{"size_limit_file"} : $outfile;
    $size_limit_file = [$size_limit_file] if $size_limit_file && !ref($size_limit_file);
    my($size_limit) = exists($opt{"size_limit"}) ? $opt{"size_limit"} : undef;
    my($dir) = exists($opt{"dir"}) ? $opt{"dir"} : undef;
    if (defined($dir) && $size_limit_file) {
        $dir =~ s{/+$}{};
        $size_limit_file = [map { m{^/} ? $_ : "$dir/$_" } @$size_limit_file];
    }
    pipe(OR, OW) or die "pipe";
    fcntl(OR, F_SETFL, fcntl(OR, F_GETFL, 0) | O_NONBLOCK);
    1 while waitpid(-1, WNOHANG) > 0;

    $run61_pid = fork();
    if ($run61_pid == 0) {
        POSIX::setpgid(0, 0) or die("child setpgid: $!\n");
        defined($dir) && chdir($dir);

        my($fn) = defined($opt{"stdin"}) ? $opt{"stdin"} : "/dev/null";
        if (defined($fn) && $fn ne "/dev/stdin") {
            my($fd) = POSIX::open($fn, O_RDONLY);
            POSIX::dup2($fd, 0);
            POSIX::close($fd) if $fd != 0;
        }

        close(OR);
        open(OW, ">", $outfile) || die if defined($outfile) && $outfile ne "pipe";
        POSIX::dup2(fileno(OW), 1);
        POSIX::dup2(fileno(OW), 2);
        close(OW) if fileno(OW) != 1 && fileno(OW) != 2;

        fcntl(STDIN, F_SETFD, fcntl(STDIN, F_GETFD, 0) & ~FD_CLOEXEC);
        fcntl(STDOUT, F_SETFD, fcntl(STDOUT, F_GETFD, 0) & ~FD_CLOEXEC);
        fcntl(STDERR, F_SETFD, fcntl(STDERR, F_GETFD, 0) & ~FD_CLOEXEC);

        { exec($command) };
        print STDERR "error trying to run $command: $!\n";
        exit(1);
    }

    POSIX::setpgid($run61_pid, $run61_pid) or die("setpgid: $!\n");

    my($before) = Time::HiRes::time();
    my($died) = 0;
    my($max_time) = exists($opt{"time_limit"}) ? $opt{"time_limit"} : 0;
    my($out, $buf, $nb) = ("", "");
    my($answer) = exists($opt{"answer"}) ? $opt{"answer"} : {};
    $answer->{"command"} = $command;

    close(OW);

    eval {
        do {
            Time::HiRes::usleep(300000);
            if (waitpid($run61_pid, WNOHANG) > 0) {
                $answer->{"status"} = $?;
                die "!";
            }
            if (defined($size_limit) && $size_limit_file && @$size_limit_file) {
                my($len) = 0;
                $out = run_sh61_pipe($out, fileno(OR), $size_limit);
                foreach my $fname (@$size_limit_file) {
                    $len += ($fname eq "pipe" ? length($out) : -s $fname);
                }
                if ($len > $size_limit) {
                    $died = "output file size $len, expected <= $size_limit";
                    die "!";
                }
            }
        } while (Time::HiRes::time() < $before + $max_time);
        $died = sprintf("timeout after %.2fs", $max_time)
            if waitpid($run61_pid, WNOHANG) <= 0;
    };

    my($delta) = Time::HiRes::time() - $before;
    $answer->{"time"} = $delta;

    if (exists($answer->{"status"}) && exists($opt{"delay"}) && $opt{"delay"} > 0) {
        Time::HiRes::usleep($opt{"delay"} * 1e6);
    }
    if (exists($opt{"nokill"})) {
        $answer->{"pgrp"} = $run61_pid;
    } else {
        kill 9, -$run61_pid;
    }
    $run61_pid = 0;

    if ($died) {
        $answer->{"killed"} = $died;
        close(OR);
        return $answer;
    }

    if (defined($outfile) && $outfile ne "pipe") {
        $out = "";
        close(OR);
        open(OR, "<", (defined($dir) ? "$dir/$outfile" : $outfile));
    }
    $answer->{"output"} = run_sh61_pipe($out, fileno(OR), $size_limit);
    close(OR);

    return $answer;
}

sub read_expected ($) {
    my($fname) = @_;
    open(EXPECTED, $fname) or die;

    my(@expected);
    my($line, $skippable, $unordered) = (0, 0, 0);
    my($allow_asan_warning, $time) = (0, 0);
    while (defined($_ = <EXPECTED>)) {
        ++$line;
        if (m{^//! \?\?\?\s+$}) {
            $skippable = 1;
        } elsif (m{^//!!UNORDERED\s+$}) {
            $unordered = 1;
        } elsif (m{^//!!TIME\s+$}) {
            $time = 1;
        } elsif (m{^//!!(ALLOW_|DISALLOW_)ASAN_WARNING\s*$}) {
            $allow_asan_warning = $1 eq "ALLOW_";
        } elsif (m{^//! }) {
            s{^....(.*?)\s*$}{$1};
            $allow_asan_warning = 1 if /^alloc count:.*fail +[1-9]/;
            my($m) = {"t" => $_, "line" => $line, "skip" => $skippable,
                      "r" => "", "match" => []};
            foreach my $x (split(/(\?\?\?|\?\?\{.*?\}(?:=\w+)?\?\?|\?\?>=\d+\?\?)/)) {
                if ($x eq "???") {
                    $m->{r} =~ s{(?:\\ )+\z}{\\s+};
                    $m->{r} .= ".*";
                } elsif ($x =~ /\A\?\?\{(.*)\}=(\w+)\?\?\z/) {
                    $m->{r} .= "(" . $1 . ")";
                    push @{$m->{match}}, $2;
                } elsif ($x =~ /\A\?\?\{(.*)\}\?\?\z/) {
                    my($contents) = $1;
                    $m->{r} =~ s{(?:\\ )+\z}{\\s+};
                    $m->{r} .= "(?:" . $contents . ")";
                } elsif ($x =~ /\A\?\?>=(\d+)\?\?\z/) {
                    my($contents) = $1;
                    $contents =~ s/\A0+(?=[1-9]|0\z)//;
                    $m->{r} =~ s{(?:\\ )+\z}{\\s+};
                    my(@dig) = split(//, $contents);
                    my(@y) = ("0*[1-9]\\d{" . (@dig) . ",}");
                    for (my $i = 0; $i < @dig; ++$i) {
                        my(@xdig) = @dig;
                        if ($i == @dig - 1) {
                            $xdig[$i] = "[" . $dig[$i] . "-9]";
                        } else {
                            next if $dig[$i] eq "9";
                            $xdig[$i] = "[" . ($dig[$i] + 1) . "-9]";
                        }
                        for (my $j = $i + 1; $j < @dig; ++$j) {
                            $xdig[$j] = "\\d";
                        }
                        push @y, "0*" . join("", @xdig);
                    }
                    $m->{r} .= "(?:" . join("|", @y) . ")(?!\\d)";
                } else {
                    $m->{r} .= quotemeta($x);
                }
            }
            push @expected, $m;
            $skippable = 0;
        }
    }
    return {"l" => \@expected, "nl" => scalar(@expected),
            "skip" => $skippable, "unordered" => $unordered, "time" => $time,
            "allow_asan_warning" => $allow_asan_warning};
}

sub read_actual ($) {
    my($fname) = @_;
    open(ACTUAL, $fname) or die;
    my(@actual);
    while (defined($_ = <ACTUAL>)) {
        chomp;
        push @actual, $_;
    }
    close ACTUAL;
    \@actual;
}

sub run_compare_print_actual ($$$) {
    my($actual, $a, $aname) = @_;
    my($apfx) = "$aname:" . ($a + 1);
    my($sep) = sprintf("$Off\n  $Redctx%" . length($apfx) . "s       ", "");
    my($context) = $actual->[$a];
    for (my $i = 2; $i <= $ContextLines && $a + 1 != @$actual; ++$a) {
        $context .= $sep . $actual->[$a + 1];
    }
    print STDERR "  $Redctx", $apfx, ": Got `", $context, "`$Off\n";
}

sub compare_test_line ($$$) {
    my($line, $expline, $chunks) = @_;
    my($rex) = $expline->{r};
    while (my($k, $v) = each %$chunks) {
        $rex =~ s{\\\?\\\?$k\\\?\\\?}{$v}g;
    }
    if ($line =~ m{\A$rex\z}) {
        for (my $i = 0; $i < @{$expline->{match}}; ++$i) {
            $chunks->{$expline->{match}->[$i]} = ${$i + 1};
        }
        return 1;
    } else {
        return 0;
    }
}

sub run_compare ($$$$$$) {
    my($actual, $exp, $aname, $ename, $outname, $out) = @_;
    my($unordered) = $exp->{unordered};
    $outname .= " " if $outname;

    my(%chunks);
    my($a) = 0;
    my(@explines) = @{$exp->{l}};
    for (; $a != @$actual; ++$a) {
        $_ = $actual->[$a];
        if (/^==\d+==\s*WARNING: AddressSanitizer failed to allocate/
            && $exp->{allow_asan_warning}) {
            next;
        }

        if (!@explines && !$exp->{skip}) {
            my($lines) = $exp->{nl} == 1 ? "line" : "lines";
            print STDERR "$Red${outname}FAIL: Too much output (expected ", $exp->{nl}, " output $lines)$Off\n";
            run_compare_print_actual($actual, $a, $aname);
            return 1;
        } elsif (!@explines) {
            next;
        }

        my($ok, $e) = (0, 0);
        while ($e != @explines) {
            $ok = compare_test_line($_, $explines[$e], \%chunks);
            last if $ok || !$unordered;
            ++$e;
        }
        if ($ok) {
            splice @explines, $e, 1;
        } elsif (!$explines[0]->{skip}) {
            print STDERR "$Red${outname}FAIL: Unexpected output starting on line ", $a + 1, "$Off\n";
            print STDERR "  $Redctx$ename:", $explines[0]->{line}, ": Expected `", $explines[0]->{t}, "`\n";
            run_compare_print_actual($actual, $a, $aname);
            return 1;
        }
    }

    if (@explines) {
        print STDERR "$Red${outname}FAIL: Missing output starting on line ", scalar(@$actual), "$Off\n";
        print STDERR "  $Redctx$ename:", $explines[0]->{line}, ": Expected `", $explines[0]->{t}, "`$Off\n";
        return 1;
    } else {
        my($ctx) = "";
        if ($exp->{time}) {
            $ctx = "$Greenctx in " . sprintf("%.03f sec", $out->{time});
        }
        print STDERR "$Green${outname}OK$ctx$Off\n";
        return 0;
    }
}

while (@ARGV > 0) {
    if ($ARGV[0] eq "-c" && @ARGV > 1 && $ARGV[1] =~ /^\d+$/) {
        $ContextLines = +$ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^-c(\d+)$/) {
        $ContextLines = +$1;
    } elsif ($ARGV[0] eq "-l") {
        $LeakCheck = 1;
    } elsif ($ARGV[0] eq "-r" && @ARGV > 1) {
        push @Restrict, $ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^-r(.+)$/) {
        push @Restrict, $1;
    } elsif ($ARGV[0] eq "-m") {
        $Make = 1;
    } elsif ($ARGV[0] eq "-e") {
        $Test = 1;
    } elsif ($ARGV[0] eq "-x" && @ARGV > 1) {
        $Exec = $ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /^-x(.+)$/) {
        $Exec = $1;
    } elsif ($ARGV[0] =~ /=/) {
        push @Makeargs, $ARGV[0];
    } elsif ($ARGV[0] =~ /\A-/) {
        print STDERR "Usage: ./check.pl [-c CONTEXT] [-l] [TESTS...]\n";
        print STDERR "       ./check.pl -x EXECFILE\n";
        print STDERR "       ./check.pl -e TESTS...\n";
        exit 1;
    } else {
        last;
    }
    shift @ARGV;
}

for (my $i = 0; $i < @ARGV; ) {
    if ($ARGV[$i] =~ /=/) {
        push @Makeargs, $ARGV[$i];
        splice @ARGV, $i, 1;
    } else {
        ++$i;
    }
}

sub test_class ($;@) {
    my($test) = shift @_;
    foreach my $x (@_) {
        if ($test eq $x
            || ($x =~ m{(?:\A|[,\s])(\d+)-(\d+)(?:[,\s]|\z)} && $test >= $1 && $test <= $2)
            || $x =~ m{(?:\A|[,\s])$test(?:[,\s]|\z)}
            || ($x =~ m{(?:\A|[,\s])san(?:[,\s]|\z)}i && $test >= 1 && $test <= 26)
            || ($x =~ m{(?:\A|[,\s])leak(?:[,\s]|\z)}i && grep { $_ == $test } (1, 8, 10, 11, 14, 15, 24, 29, 35, 36, 37, 39))) {
            return 1;
        }
    }
    0;
}

sub asan_options ($) {
    my($test) = @_;
    $test = int($1) if $test =~ m{\A(?:\./)?test(\d+)\z};
    if ($LeakCheck && test_class($test, "leak")) {
        return "allocator_may_return_null=1 detect_leaks=1";
    } else {
        return "allocator_may_return_null=1 detect_leaks=0";
    }
}

sub test_runnable ($) {
    my($number) = @_;
    foreach my $r (@Restrict) {
        return 0 if !test_class($number, $r);
    }
    return !@ARGV || test_class($number, @ARGV);
}

if ($Exec) {
    die "bad -x option\n" if $Exec !~ m{\A(?:\./)?[^./][^/]+\z};
    $ENV{"ASAN_OPTIONS"} = asan_options($Exec);
    $out = run_sh61("./" . $Exec, "stdout" => "pipe", "stdin" => "/dev/null",
                    "time_limit" => 10, "size_limit" => 80000);
    my($ofile) = "out/" . $Exec . ".output";
    if (open(OUT, ">", $ofile)) {
        print OUT $out->{output};
        close OUT;
    }
    exit(run_compare([split("\n", $out->{"output"})],
                     read_expected($Exec . ".cc"),
                     $ofile, $Exec . ".cc", $Exec, $out));
} else {
    my($maxtest, $ntest, $ntestfailed) = (40, 0, 0);
    if ($Test) {
        for ($i = 1; $i <= $maxtest; $i += 1) {
            printf "test%03d\n", $i if test_runnable($i)
        }
        exit;
    }
    if ($Make) {
        my(@makeargs) = ("make", @Makeargs);
        for ($i = 1; $i <= $maxtest; $i += 1) {
            push @makeargs, sprintf("test%03d", $i) if test_runnable($i);
        }
        system(@makeargs);
        exit 1 if $? != 0;
    }
    $ENV{"MALLOC_CHECK_"} = 0;
    for ($i = 1; $i <= $maxtest; $i += 1) {
        next if !test_runnable($i);
        ++$ntest;
        $ENV{"ASAN_OPTIONS"} = asan_options($i);
        printf STDERR "test%03d ", $i;
        $out = run_sh61(sprintf("./test%03d", $i), "stdout" => "pipe", "stdin" => "/dev/null",
                        "time_limit" => $i == 35 ? 10 : 5, "size_limit" => 8000);
        if (exists($out->{killed})) {
            print STDERR "${Red}CRASH: $out->{killed}$Off\n";
            if (exists($out->{output}) && $out->{output} =~ m/\A\s*(.+)/) {
                print STDERR "  ${Redctx}1st line of output: $1$Off\n";
            }
            ++$ntestfailed;
        } else {
            ++$ntestfailed if run_compare([split("\n", $out->{"output"})],
                    read_expected(sprintf("test%03d.cc", $i)),
                    "output", sprintf("test%03d.cc", $i), "", $out);
        }
    }
    my($ntestpassed) = $ntest - $ntestfailed;
    if ($ntest == $maxtest && $ntestpassed == $ntest) {
        print STDERR "${Green}All tests passed!$Off\n";
    } else {
        my($color) = ($ntestpassed == 0 ? $Red : ($ntestpassed == $ntest ? $Green : $Cyan));
        print STDERR "${color}$ntestpassed of $ntest ", ($ntest == 1 ? "test" : "tests"), " passed$Off\n";
    }
}
