#!/usr/bin/ruby

# some OSes are finicky about the order of included header
# files.  this script looks through the .c files in wmbiff
# to verify that some headers (like sys/types) are included
# before others.

$failed = 0
FILES = 'wmgeneral/*.[ch] wmbiff/*.[ch]'

def include_a_before_b(first, second) 
    typeincl = Hash.new(9000)
    IO.popen("egrep -n \"#{first}[>.]\" " + FILES, 'r').readlines.each { |l|
        file,line,text = l.split(':');
        typeincl[file] = line;
    }
    
    IO.popen("grep -n \"#{second}[>.]\" " + FILES, 'r').readlines.each { |l|
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
end

include_a_before_b("sys/types", "sys/socket") 
include_a_before_b("sys/types", "netinet/in_systm") 
include_a_before_b("netinet/in_systm", "netinet/ip") 
include_a_before_b("netinet/in", "netinet/ip") 
include_a_before_b("sys/types", "sys/resource")  # not sure if there's more 
include_a_before_b("sys/time", "sys/resource")   # order here.

Kernel.exit($failed)
