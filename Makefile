#
# Copyright (C) 2010-2012 OpenWrt.org
#
# This Makefile and the code shipped in src/ is free software, licensed
# under the GNU Lesser General Public License, version 2.1 and later.
# See src/COPYING for more information.
#
# Refer to src/COPYRIGHT for copyright statements on the source files.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mbox
PKG_VERSION:=V1.0.0
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_LICENSE:=FREE
PKG_LICENSE_FILES:=LICENSE

PKG_MAINTAINER:=jack chen<jk110333@126.com>

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/target.mk

define Package/mbox
    SECTION:=base
    CATEGORY:=Persiancat
    TITLE:=mqtt client mbox tool
    DEPENDS:=+libmosquitto +libpthread +libjson-c +libuci
endef

TARGET_CFLAGS += \
		 -DVER="\\\"$(SUBTARGET)-$(PKG_VERSION)\\\""
define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/mbox/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mbox $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/mbox $(1)/etc/config
endef

$(eval $(call BuildPackage,mbox))
