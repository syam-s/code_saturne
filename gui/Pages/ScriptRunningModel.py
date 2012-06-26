# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------------

# This file is part of Code_Saturne, a general-purpose CFD tool.
#
# Copyright (C) 1998-2012 EDF S.A.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.

#-------------------------------------------------------------------------------

"""
This module modifies the parameters used by the script file
- ScriptRunningModel
"""
#-------------------------------------------------------------------------------
# Standard modules import
#-------------------------------------------------------------------------------

import sys, unittest
import os, os.path, shutil, sys, string, types, re

#-------------------------------------------------------------------------------
# Library modules import
#-------------------------------------------------------------------------------

import Base.Toolbox as Tool
from Pages.SolutionDomainModel import MeshModel, SolutionDomainModel
from Pages.AtmosphericFlowsModel import AtmosphericFlowsModel
from Base.XMLvariables import Variables, Model

#-------------------------------------------------------------------------------
# Class ScriptRunningModel
#-------------------------------------------------------------------------------

class ScriptRunningModel(Model):
    """
    This class modifies the script file (runcase)
    """
    def __init__(self, case):
        """
        Constructor.
        """
        self.case = case
        self.node_mgt = self.case.xmlInitNode('calculation_management')

        self.parameters = None


    def getRunType(self):
        """
        Get run type.
        """
        val = self.node_mgt.xmlGetString('run_type')
        if not val:
            val = 'standard'

        return val


    def setRunType(self, run):
        """
        Set run type.
        """
        self.isInList(run, ('none', 'mesh preprocess', 'mesh quality', 'standard'))
        self.node_mgt.xmlSetData('run_type', run)


    def getLogType(self):
        """
        Get logging options.
        """
        log_type = ['listing', 'null']

        node = self.node_mgt.xmlGetNode('logging')
        if node:
            log_type[0] = node['main']
            log_type[1] = node['parallel']

        if not log_type[0]:
            if self.case['salome']:
                log_type[0] = 'stdout'
            else:
                log_type[0] = 'listing'

        if not log_type[1]:
            log_type[1] = 'null'

        return log_type


    def setLogType(self, log_type):
        """
        Set logging options.
        """
        if log_type[0] == 'listing' and not self.case['salome']:
            log_type[0] = None
        if log_type[1] == 'null':
            log_type[1] = None

        node = self.node_mgt.xmlInitNode('logging')

        if not log_type[0] and not log_type[1]:
            if node:
                node.xmlRemoveNode()
        else:
            if log_type[0]:
                node['main'] = log_type[0]
            else:
                del node['main']
            if log_type[1]:
                node['parallel'] = log_type[1]
            else:
                del node['parallel']


    def getUserInputFiles(self):
        """
        Get user data file names.
        """
        input_files = []
        node = self.node_mgt.xmlGetNode('user_input_files')
        if node:
            nodeList = node.xmlGetNodeList('data', 'name')
            for node in nodeList:
                input_files.append(node['name'])
        return input_files


    def setUserInputFiles(self, input_files):
        """
        Set user input files.
        """
        if not input_files:
            node = self.node_mgt.xmlGetNode('user_input_files')
            if node:
                node.xmlRemoveNode()
        else:
            node = self.node_mgt.xmlInitNode('user_input_files')
            for old in node.xmlGetNodeList('data', 'name'):
                old.xmlRemoveNode()
            for f in input_files:
                node.xmlInitNode('data', name=f)


    def getString(self, key):
        """
        Get entry by named string.
        """
        val = self.node_mgt.xmlGetString(key)
        if not val:
            val = ''

        return val


    def setString(self, key, string):
        """
        Set entry by named string.
        """
        if not string:
            node = self.node_mgt.xmlGetNode(key)
            if node:
                node.xmlRemoveNode()
        else:
            self.node_mgt.xmlSetData(key, string)


#-------------------------------------------------------------------------------
# ScriptRunningModel test class
#-------------------------------------------------------------------------------

class ScriptRunningModelTestCase(unittest.TestCase):
    """
    """
    def setUp(self):
        """
        This method is executed before all 'check' methods.
        """
        from Base.XMLengine import Case
        from Base.XMLinitialize import XMLinit
        from Base.Toolbox import GuiParam
        GuiParam.lang = 'en'
        self.case = Case(None)
        XMLinit(self.case).initialize()

        domain = SolutionDomainModel(self.case)
        domain.addMesh('mail1.des', 'des')
        domain.addMesh('mail2.des', 'des')
        domain.addMesh('mail3.des', 'des')
        domain.setOrientation('on')

        self.case['xmlfile'] = 'NEW.xml'

        self.f = open('runcase_test','w')
        self.f.write(runcase_test)
        self.f.close()


def suite():
    testSuite = unittest.makeSuite(ScriptRunningModelTestCase, "check")
    return testSuite


def runTest():
    print("ScriptRunningModelTestCase")
    runner = unittest.TextTestRunner()
    runner.run(suite())


#-------------------------------------------------------------------------------
# End of ScriptRunningModel
#-------------------------------------------------------------------------------
