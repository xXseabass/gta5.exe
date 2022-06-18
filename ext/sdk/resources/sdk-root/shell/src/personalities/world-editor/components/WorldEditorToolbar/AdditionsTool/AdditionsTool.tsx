import React from 'react';
import classnames from 'classnames';
import { WETool } from '../../../store/WEToolbarState';
import { BaseTool } from '../BaseTool/BaseTool';
import { useOpenFlag } from 'utils/hooks';
import { WEState } from 'personalities/world-editor/store/WEState';
import { DropTargetMonitor, useDrop } from 'react-dnd';
import { ADDITION_DND_TYPES } from './AdditionsTool.constants';
import { WORLD_EDITOR_MAP_NO_GROUP } from 'backend/world-editor/world-editor-constants';
import { newDirectoryIcon } from 'fxdk/ui/icons';
import { ContextMenu, ContextMenuItemsCollection } from 'fxdk/ui/controls/ContextMenu/ContextMenu';
import { InPlaceInput } from 'fxdk/ui/controls/InPlaceInput/InPlaceInput';
import { Addition } from './Addition';
import { AdditionsGroup } from './AdditionsGroup/AdditionsGroup';
import { AdditionsNewGroup } from './AdditionsNewGroup/AdditionsNewGroup';
import { observer } from 'mobx-react-lite';
import { WESelectionType } from 'backend/world-editor/world-editor-types';
import { additionsToolIcon } from 'personalities/world-editor/constants/icons';
import { IntroForceRecalculate } from 'fxdk/ui/Intro/Intro';
import s from './AdditionsTool.module.scss';

export const AdditionsTool = observer(function AdditionsTool() {
  const [groupCreatorOpen, openGroupCreator, closeGroupCreator] = useOpenFlag(false);

  const handleGroupNameChange = React.useCallback((newGroupName: string) => {
    closeGroupCreator();

    if (newGroupName) {
      WEState.map!.createAdditionGroup(newGroupName);
    }
  }, [closeGroupCreator]);

  const [{ isDropping }, dropRef] = useDrop({
    accept: ADDITION_DND_TYPES.ADDITION,
    drop(item: unknown | { id: string, type: string }, monitor: DropTargetMonitor) {
      if (monitor.didDrop()) {
        return;
      }

      if (typeof item === 'object' && item?.['id']) {
        WEState.map!.setAdditionGroup(item['id'], WORLD_EDITOR_MAP_NO_GROUP);
      }
    },
    collect: (monitor) => ({
      isDropping: monitor.isOver({ shallow: true }) && monitor.canDrop(),
    }),
  });

  const rootClassName = classnames(s.root, {
    [s.dropping]: isDropping,
  });

  const contextMenu = React.useMemo(() => [
    {
      id: 'create-group',
      text: 'Create group',
      icon: newDirectoryIcon,
      onClick: openGroupCreator,
    },
  ] as ContextMenuItemsCollection, [openGroupCreator]);

  const hasGroups = Object.keys(WEState.map!.additionGroups).length > 0;
  const hasAdditions = Object.keys(WEState.map!.additions).length > 0;

  const showPlaceholder = !hasGroups && !hasAdditions;

  return (
    <BaseTool
      renderAlways
      tool={WETool.Additions}
      icon={additionsToolIcon}
      label="Map additions"
      highlight={WEState.selection.type === WESelectionType.ADDITION}
      toggleProps={{ 'data-intro-id': 'map-additions' }}
      panelProps={{ 'data-intro-id': 'map-additions' }}
    >
      <IntroForceRecalculate />

      <ContextMenu
        ref={dropRef}
        className={rootClassName}
        activeClassName={s.active}
        items={contextMenu}
      >
        <AdditionsNewGroup />

        {groupCreatorOpen && (
          <div className={s.creator}>
            {newDirectoryIcon}
            <InPlaceInput
              value=""
              onChange={handleGroupNameChange}
              placeholder="New group name"
            />
          </div>
        )}

        {Object.keys(WEState.map!.additionGroups).map((grp) => {
          return (
            <AdditionsGroup
              key={grp}
              grp={grp}
              items={WEState.map!.getGroupAdditions(grp)}
            />
          );
        })}

        {showPlaceholder && (
          <div className={s.placeholder}>
            Create addition by clicking plus button above
          </div>
        )}

        {Object.keys(WEState.map!.additionsUngrouped).map((id) => (
          <Addition
            id={id}
            key={id}
          />
        ))}
      </ContextMenu>
    </BaseTool>
  );
});
