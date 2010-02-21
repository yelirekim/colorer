use strict;
package Helper::Class;

our $VERSION = 0.03;


# virtual constructor
#
# using (in subclass):
#
# use base qw(Helper::Class);
# use Helper::Class;
#
# sub new
# {
# 	return object @_[0], key1 => 'value2', key2 => 'value2' ....
# }
#

sub new
{
	my $class = shift;
	my $this = { @_ };
	bless($this, $class);
	return $this;
}


# clone, for deep structs must be redefined
#
# my $foo = $bar->clone(); # copy $bar to $foo

sub clone
{
	my $prot = shift;
	return undef unless $prot->isa(__PACKAGE__);
	
	my $class = ref($prot);
	my $this = { %{$prot} };
	bless $this, $class;
	return $this;
}


# virtual static member access (return typeglob ref)
#
# using:
#	my $m = $obj->static('member')
#	print $$m;	# prints $obj::member;
#	print @$m;	# prints @obj::member;
#	....
#

sub static_pack
{
	my ($pack, $varname) = @_;
	my $class = packref($pack);
	
	return $class->{$varname} if defined $class->{$varname};
	return unless defined $class->{ISA};
	
	my @isa = @{$class->{ISA}};
	
	foreach(@isa)
	{
		my $ret = static_pack($_, $varname);
		return $ret if $ret;
	}
	return undef;
}


sub static 
{
	my ($this, $varname) = @_;
	
	return static_pack(ref($this), $varname);
}



# convert package name to package hash

sub packref
{
	my $class = shift;
	
	no strict 'refs';
	return \%{$class.'::'};
}


# convert blessed ref to package hash

sub classref
{
	my $this = shift;
	return ref($this)->packref;
}



# initial import
#
# NOTE: you MUST decare "new" BEFORE "use <childs>"!
#

sub import
{
	my $call = caller(0);
	return unless $call->isa(__PACKAGE__);
	
	no strict 'refs';
	my @isa = @{${$call.'::'}{ISA}};
	
	*{$call.'::object'} = \&new if grep{$_ eq __PACKAGE__} @isa;
	
	if(my $new = $isa[0]->can('new'))
	{
		*{$call.'::super'} = $new;
	}
	
	return unless $#isa;
	for(0..$#isa)
	{
		if(my $new = $isa[$_]->can('new'))
		{
			*{$call.'::super'.($_+1)} = $new;
		}
	}
}

1;

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is DTD2XML.
#
# The Initial Developer of the Original Code is
# Eugene Efremov <4mirror@mail.ru>
# Portions created by the Initial Developer are Copyright (C) 2009-2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** 
