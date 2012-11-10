/*
 * Copyright (C) 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.doclava;

import com.google.doclava.apicheck.ApiCheck;
import com.google.doclava.apicheck.ApiInfo;
import com.google.doclava.apicheck.ApiParseException;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * Cross-references documentation among different libraries. A FederationTagger
 * is populated with a list of {@link FederatedSite} objects which are linked
 * against when overlapping content is discovered.
 */
public final class FederationTagger {
  private final List<FederatedSite> federatedSites = new ArrayList<FederatedSite>();
  
  /**
   * Adds a Doclava documentation site for federation. Accepts the base URL of
   * the remote API.
   */
  public void addSite(String name, URL site) {
    try {
      federatedSites.add(new FederatedSite(name, site));
    } catch (ApiParseException e) {
      String error = "Could not add site for federation: " + site;
      if (e.getMessage() != null) {
        error += ": " + e.getMessage();
      }
      Errors.error(Errors.NO_FEDERATION_DATA, null, error);
    }
  }
  
  public void tagAll(ClassInfo[] classDocs) {
    for (FederatedSite site : federatedSites) {
      applyFederation(site, classDocs);
    }
  }
  
  private void applyFederation(FederatedSite federationSource, ClassInfo[] classDocs) {
    for (ClassInfo classDoc : classDocs) {
      PackageInfo packageSpec
          = federationSource.apiInfo().getPackages().get(classDoc.containingPackage().name());

      if (packageSpec == null) {
        continue;
      }

      ClassInfo classSpec = packageSpec.allClasses().get(classDoc.name());
      
      if (classSpec == null) {
        continue;
      }
      
      federateMethods(federationSource, classSpec, classDoc);
      federateConstructors(federationSource, classSpec, classDoc);
      federateFields(federationSource, classSpec, classDoc);
      federateClass(federationSource, classDoc);
      federatePackage(federationSource, classDoc.containingPackage());
    }
  }

  private void federateMethods(FederatedSite site, ClassInfo federatedClass, ClassInfo localClass) {
    for (MethodInfo method : localClass.methods()) {
      for (ClassInfo superclass : federatedClass.hierarchy()) {
        if (superclass.allMethods().containsKey(method.getHashableName())) {
          method.addFederatedReference(site);
          break;
        }
      }
    }
  }
  
  private void federateConstructors(FederatedSite site, ClassInfo federatedClass,
      ClassInfo localClass) {
    for (MethodInfo constructor : localClass.constructors()) {
      if (federatedClass.hasConstructor(constructor)) {
        constructor.addFederatedReference(site);
      }
    }
  }
  
  private void federateFields(FederatedSite site, ClassInfo federatedClass, ClassInfo localClass) {
    for (FieldInfo field : localClass.fields()) {
      if (federatedClass.allFields().containsKey(field.name())) {
        field.addFederatedReference(site);
      }
    }
  }
  
  private void federateClass(FederatedSite source, ClassInfo doc) {
    doc.addFederatedReference(source);
  }
  
  private void federatePackage(FederatedSite source, PackageInfo pkg) {
    pkg.addFederatedReference(source);
  }
}