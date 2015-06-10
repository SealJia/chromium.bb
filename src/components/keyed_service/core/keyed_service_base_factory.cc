// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_base_factory.h"

#include "base/prefs/pref_service.h"
#include "base/supports_user_data.h"
#include "base/trace_event/trace_event.h"
#include "components/keyed_service/core/dependency_manager.h"

void KeyedServiceBaseFactory::RegisterUserPrefsOnContextForTest(
    base::SupportsUserData* context) {
  TRACE_EVENT0("browser,startup",
               "KeyedServiceBaseFactory::RegisterUserPrefsOnContextForTest");
  // Safe timing for pref registration is hard. Previously, we made
  // context responsible for all pref registration on every service
  // that used contexts. Now we don't and there are timing issues.
  //
  // With normal contexts, prefs can simply be registered at
  // DependencyManager::RegisterProfilePrefsForServices time.
  // With incognito contexts, we just never register since incognito
  // contexts share the same pref services with their parent contexts.
  //
  // Testing contexts throw a wrench into the mix, in that some tests will
  // swap out the PrefService after we've registered user prefs on the original
  // PrefService. Test code that does this is responsible for either manually
  // invoking RegisterProfilePrefs() on the appropriate
  // ContextKeyedServiceFactory associated with the prefs they need,
  // or they can use SetTestingFactory() and create a service (since service
  // creation with a factory method causes registration to happen at
  // TestingProfile creation time).
  //
  // Now that services are responsible for declaring their preferences, we have
  // to enforce a uniquenes check here because some tests create one context and
  // multiple services of the same type attached to that context (serially, not
  // parallel) and we don't want to register multiple times on the same context.
  // This is the purpose of RegisterProfilePrefsIfNecessary() which could be
  // replaced directly by RegisterProfilePrefs() if this method is ever phased
  // out.
  RegisterPrefsIfNecessaryForContext(context,
                                     GetAssociatedPrefRegistry(context));
}

KeyedServiceBaseFactory::KeyedServiceBaseFactory(const char* service_name,
                                                 DependencyManager* manager)
    : dependency_manager_(manager) {
#ifndef NDEBUG
  service_name_ = service_name;
#endif
  dependency_manager_->AddComponent(this);
}

KeyedServiceBaseFactory::~KeyedServiceBaseFactory() {
  dependency_manager_->RemoveComponent(this);
}

void KeyedServiceBaseFactory::DependsOn(KeyedServiceBaseFactory* rhs) {
  dependency_manager_->AddEdge(rhs, this);
}

void KeyedServiceBaseFactory::RegisterPrefsIfNecessaryForContext(
    const base::SupportsUserData* context,
    user_prefs::PrefRegistrySyncable* registry) {
  if (!ArePreferencesSetOn(context)) {
    RegisterPrefs(registry);
    MarkPreferencesSetOn(context);
  }
}

#ifndef NDEBUG
void KeyedServiceBaseFactory::AssertContextWasntDestroyed(
    const base::SupportsUserData* context) const {
  DCHECK(CalledOnValidThread());
  dependency_manager_->AssertContextWasntDestroyed(context);
}

void KeyedServiceBaseFactory::MarkContextLiveForTesting(
    const base::SupportsUserData* context) {
  DCHECK(CalledOnValidThread());
  dependency_manager_->MarkContextLiveForTesting(context);
}
#endif

bool KeyedServiceBaseFactory::ServiceIsCreatedWithContext() const {
  return false;
}

bool KeyedServiceBaseFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

void KeyedServiceBaseFactory::ContextDestroyed(
    base::SupportsUserData* context) {
  // While object destruction can be customized in ways where the object is
  // only dereferenced, this still must run on the UI thread.
  DCHECK(CalledOnValidThread());

  registered_preferences_.erase(context);
}

bool KeyedServiceBaseFactory::ArePreferencesSetOn(
    const base::SupportsUserData* context) const {
  return registered_preferences_.find(context) != registered_preferences_.end();
}

void KeyedServiceBaseFactory::MarkPreferencesSetOn(
    const base::SupportsUserData* context) {
  DCHECK(!ArePreferencesSetOn(context));
  registered_preferences_.insert(context);
}
