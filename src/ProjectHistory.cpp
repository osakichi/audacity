/**********************************************************************

Audacity: A Digital Audio Editor

ProjectHistory.cpp

Paul Licameli split from ProjectManager.cpp

**********************************************************************/

#include "ProjectHistory.h"

#include "Project.h"
#include "ProjectFileIO.h"
#include "Tags.h"
#include "Track.h"
#include "UndoManager.h"
#include "ViewInfo.h"
#include "WaveTrack.h"

static AudacityProject::AttachedObjects::RegisteredFactory sProjectHistoryKey {
   []( AudacityProject &project ) {
      return std::make_shared< ProjectHistory >( project );
   }
};

ProjectHistory &ProjectHistory::Get( AudacityProject &project )
{
   return project.AttachedObjects::Get< ProjectHistory >( sProjectHistoryKey );
}

const ProjectHistory &ProjectHistory::Get( const AudacityProject &project )
{
   return Get( const_cast< AudacityProject & >( project ) );
}

ProjectHistory::~ProjectHistory() = default;

//
// Undo/History methods
//

void ProjectHistory::InitialState()
{
   auto &project = mProject;
   auto &tracks = TrackList::Get( project );
   auto &viewInfo = ViewInfo::Get( project );
   auto &undoManager = UndoManager::Get( project );
   auto &tags = Tags::Get( project );

   undoManager.ClearStates();

   undoManager.PushState(
      &tracks, viewInfo.selectedRegion, tags.shared_from_this(),
      XO("Created new project"), {});

   undoManager.StateSaved();
}

bool ProjectHistory::UndoAvailable() const
{
   auto &project = mProject;
   auto &tracks = TrackList::Get( project );
   auto &undoManager = UndoManager::Get( project );
   return undoManager.UndoAvailable() &&
       !tracks.HasPendingTracks();
}

bool ProjectHistory::RedoAvailable() const
{
   auto &project = mProject;
   auto &tracks = TrackList::Get( project );
   auto &undoManager = UndoManager::Get( project );
   return undoManager.RedoAvailable() &&
      !tracks.HasPendingTracks();
}

void ProjectHistory::PushState(
   const TranslatableString &desc, const TranslatableString &shortDesc)
{
   PushState(desc, shortDesc, UndoPush::AUTOSAVE);
}

void ProjectHistory::PushState(const TranslatableString &desc,
                                const TranslatableString &shortDesc,
                                UndoPush flags )
{
   auto &project = mProject;
   auto &projectFileIO = ProjectFileIO::Get( project );
   auto &tracks = TrackList::Get( project );
   auto &viewInfo = ViewInfo::Get( project );
   auto &undoManager = UndoManager::Get( project );
   auto &tags = Tags::Get( project );
   undoManager.PushState(
      &tracks, viewInfo.selectedRegion, tags.shared_from_this(),
      desc, shortDesc, flags);

   mDirty = true;

   if((flags & UndoPush::AUTOSAVE) != UndoPush::MINIMAL)
      projectFileIO.AutoSave();
}

void ProjectHistory::RollbackState()
{
   auto &project = mProject;
   auto &undoManager = UndoManager::Get( project );
   SetStateTo( undoManager.GetCurrentState() );
}

void ProjectHistory::ModifyState(bool bWantsAutoSave)
{
   auto &project = mProject;
   auto &projectFileIO = ProjectFileIO::Get( project );
   auto &tracks = TrackList::Get( project );
   auto &viewInfo = ViewInfo::Get( project );
   auto &undoManager = UndoManager::Get( project );
   auto &tags = Tags::Get( project );
   undoManager.ModifyState(
      &tracks, viewInfo.selectedRegion, tags.shared_from_this());
   if (bWantsAutoSave)
      projectFileIO.AutoSave();
}

// LL:  Is there a memory leak here as "l" and "t" are not deleted???
// Vaughan, 2010-08-29: No, as "l" is a TrackList* of an Undo stack state.
//    Need to keep it and its tracks "t" available for Undo/Redo/SetStateTo.
void ProjectHistory::PopState(const UndoState &state)
{
   auto &project = mProject;
   auto &projectFileIO = ProjectFileIO::Get( project );
   auto &dstTracks = TrackList::Get( project );
   auto &viewInfo = ViewInfo::Get( project );

   viewInfo.selectedRegion = state.selectedRegion;

   // Restore tags
   Tags::Set( project, state.tags );

   TrackList *const tracks = state.tracks.get();

   dstTracks.Clear();

   for (auto t : tracks->Any())
   {
      dstTracks.Add(t->Duplicate());
   }

   projectFileIO.AutoSave();
}

void ProjectHistory::SetStateTo(unsigned int n)
{
   auto &project = mProject;
   auto &undoManager = UndoManager::Get( project );

   undoManager.SetStateTo(n,
      [this]( const UndoState &state ){ PopState(state); } );
}
