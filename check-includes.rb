#!/usr/bin/ruby

puts "Running check-includes..."

$failed = 0
FILES = Dir.glob('wmgeneral/*.[ch] wmbiff/*.[ch]').join(" ")

if FILES.empty? then
  puts "no files to check?"
  exit(0)
elsif FILES.length == 1 then
  puts "only one file to check; you may be in a VPATH build where this will not help you."
  exit(0)
end

$testnumber = 0;

def include_a_before_b(first, second) 
  puts "Testing \##{$testnumber+=1}: #{first} before #{second}..."
  typeincl = Hash.new(9000)
  IO.popen('egrep -n "include *[<\"]' + first + '[>.]" ' + FILES, 'r') { |gout|
    gout.readlines.each { |l|
        file,line,text = l.split(':');
        typeincl[file] = line;
    }
  }
    
  IO.popen('grep -n "include *[<\"]' + second + '[>.]" ' + FILES, 'r') { |gout|
    gout.readlines.each { |l|
        file,line,text = l.split(':');
        if(typeincl[file].to_i > line.to_i) then
            $failed = 1;
            if( typeincl[file].to_i < 9000 ) then
              puts "#{file}:#{typeincl[file].to_i} doesn't include #{first} (line #{typeincl[file].to_i}) before #{second}"
            else
              puts "#{file}:#{line.to_i} doesn't include #{first} though it includes #{second}"
            end
        end
    }
  }
end

def unportable_function(fn)
  puts "Testing \##{$testnumber+=1}: #{fn}() is not used..."
  typeincl = Hash.new(9000)
  IO.popen("egrep -n \"#{fn} *\\(\" " + FILES, 'r') { |gout|
    gout.readlines.each { |l|
      file,line,text = l.split(':');
      typeincl[file] = line;
      $failed = 1
    }
  }
  typeincl.each { |f,l| 
    puts "#{f}:#{l} references unportable function #{fn}"
  }
end

def unportable_constant(cn)
  puts "Testing \##{$testnumber+=1}: #{cn} is not used..."
  typeincl = Hash.new(9000)
  IO.popen("egrep -n #{cn} " + FILES, 'r') { |gout| 
    gout.readlines.each { |l|
      file,line,text = l.split(':');
      if file =~ /^\d+$/ then
        puts "internal error parsing grep output line: #{l.chomp}"
      elsif text !~ Regexp.new("/\\*.*#{cn}.*\\*/") && file !~ /config.h/ then
        # okay if the constant appears in a comment saying we're not supposed
        # to use it, or if it's in config.h (assume it's a portability fix)
        typeincl[file] = line;
        $failed = 1
      end
    }
  }
  typeincl.each { |f,l| 
    puts "#{f}:#{l} references unportable constant #{cn}"
  }
end
  

include_a_before_b("sys/types", "sys/socket") 
include_a_before_b("sys/types", "fcntl") 
include_a_before_b("sys/types", "dirent") # mac
include_a_before_b("sys/types", "netinet/in") # mac
include_a_before_b("sys/types", "netinet/in_systm") 
include_a_before_b("netinet/in_systm", "netinet/ip") 
include_a_before_b("netinet/in", "netinet/ip") 
include_a_before_b("netinet/in", "netinet/if_ether") # bsd
include_a_before_b("sys/socket", "netinet/if_ether") # bsd
include_a_before_b("sys/types", "netinet/if_ether") # bsd
include_a_before_b("sys/types", "sys/resource")  # not sure if there's more 
include_a_before_b("sys/time", "sys/resource")   # order here.
# also, nscommon should follow system header files like dirent, stdlib.

unportable_function("bcopy"); # not present on solaris
unportable_function("bzero"); # would rather just use memcpy, memset
unportable_function("asprintf"); # easy enough to malloc(strlen())
unportable_constant("ICMP_TIMESTAMP"); # use TSTAMP instead.
unportable_constant("uint64_t"); # use u_int64_t instead.
unportable_constant("uint32_t"); # use u_int32_t instead.

Kernel.exit($failed)
