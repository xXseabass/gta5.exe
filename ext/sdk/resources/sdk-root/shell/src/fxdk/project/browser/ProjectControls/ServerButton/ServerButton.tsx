import React from 'react';
import classnames from 'classnames';
import { observer } from 'mobx-react-lite';
import { BsExclamationDiamondFill, BsExclamationTriangleFill, BsPlay, BsStop } from 'react-icons/bs';
import { ServerUpdateStates } from 'shared/api.types';
import { Indicator } from 'fxdk/ui/Indicator/Indicator';
import { ServerState } from 'store/ServerState';
import { Title } from 'fxdk/ui/controls/Title/Title';
import { Project } from '../../state/project';
import s from './ServerButton.module.scss';


export const ServerButton = observer(function Server({ className }: { className: string }) {
  const updateChannelState = ServerState.getUpdateChannelState(Project.manifest.serverUpdateChannel);

  const rootClassName = classnames(s.root, className, {
    [s.up]: ServerState.isUp,
    [s.down]: ServerState.isDown,
    [s.booting]: ServerState.isBooting,
    [s.error]: updateChannelState === ServerUpdateStates.missingArtifact,
  });

  let icon;
  let title;

  if (updateChannelState === ServerUpdateStates.ready) {
    switch (true) {
      case ServerState.isUp: {
        icon = <BsStop />;
        title = 'Stop server';
        break;
      }

      case ServerState.isDown: {
        icon = <BsPlay />;
        title = 'Start server';
        break;
      }

      case ServerState.isBooting: {
        icon = <Indicator />;
        title = 'Stop server';
        break;
      }
    }
  } else {
    switch (updateChannelState) {
      case ServerUpdateStates.missingArtifact: {
        icon = <BsExclamationDiamondFill />;
        title = 'Missing server artifact';
        break;
      }

      case ServerUpdateStates.updateRequired: {
        icon = <BsExclamationTriangleFill />;
        title = 'Install update';
        break;
      }

      case ServerUpdateStates.checking: {
        icon = <Indicator />;
        title = 'Checking updates';
        break;
      }

      case ServerUpdateStates.updating: {
        icon = <Indicator />;
        title = 'Updating';
      }
    }
  }

  const handleClick = React.useCallback(() => {
    if (updateChannelState === ServerUpdateStates.updateRequired) {
      return ServerState.installUpdate(Project.manifest.serverUpdateChannel);
    }

    if (updateChannelState !== ServerUpdateStates.ready) {
      return;
    }

    ServerState.toggleServer();
  }, [updateChannelState]);

  return (
    <Title animated={false} delay={0} title={title} fixedOn="bottom">
      {(ref) => (
        <button
          ref={ref}
          className={rootClassName}
          onClick={handleClick}
          data-intro-id="start-server-button"
        >
          {icon}
        </button>
      )}
    </Title>
  );
});
