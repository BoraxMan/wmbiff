# $Id: changelog.sed,v 1.5 2002/01/27 20:20:48 jordi Exp $
# Changes cvs2cl's output to something more pleasant
s/\([0-9][0-9]:[0-9][0-9]\) /\1 UTC --/g

# Add yourself below:
s/ dwonis/ Dwayne C. Litzenberger <dlitz@dlitz.net>/g
s/ oskuro/ Jordi Mallach <jordi@sindominio.net>/g
s/ jordi/ Jordi Mallach <jordi@sindominio.net>/g
s/ markph/ Mark Hurley <debian4tux@telocity.com>/g
s/ bluehal/ Neil Spring <nspring@cs.washington.edu>/g
