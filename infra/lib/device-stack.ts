import * as cdk from 'aws-cdk-lib';
import * as iot from 'aws-cdk-lib/aws-iot';
import { Construct } from 'constructs';

export interface DeviceStackProps extends cdk.StackProps {
  deployEnv: 'sandbox' | 'prod';
  /** One IoT Thing per physical Katooni, e.g. ['unit-1']. */
  deviceNames: string[];
}

/**
 * KATOO-10 (from tinycrypt's KeyStack, CRYPT-14/15): each Katooni is an IoT Thing that may only
 * connect as itself, take Fleet's commands on katooni/{thing}/commands and answer on
 * katooni/{thing}/responses (its own only). Its attributes make it show up in Fleet (nakomis-fleet)
 * as project katooni, and make Fleet offer it signed builds only. Its certificate is made and
 * attached by infra/scripts/provision-device.sh, not here: the private key must never pass through
 * CloudFormation.
 */
export class DeviceStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props: DeviceStackProps) {
    super(scope, id, props);
    const { deployEnv, deviceNames } = props;

    const arn = (resource: string) => `arn:${this.partition}:iot:${this.region}:${this.account}:${resource}`;

    // Policy variables pin each certificate to its own Thing: the client id must be the Thing's name,
    // and the certificate must be attached to that Thing.
    const policy = new iot.CfnPolicy(this, 'DevicePolicy', {
      policyName: `katooni-device-${deployEnv}`,
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          {
            Effect: 'Allow',
            Action: 'iot:Connect',
            Resource: arn('client/${iot:Connection.Thing.ThingName}'),
            Condition: { Bool: { 'iot:Connection.Thing.IsAttached': 'true' } },
          },
          {
            Effect: 'Allow',
            Action: 'iot:Publish',
            Resource: arn('topic/katooni/${iot:Connection.Thing.ThingName}/responses'),
          },
          {
            Effect: 'Allow',
            Action: 'iot:Subscribe',
            Resource: arn('topicfilter/katooni/${iot:Connection.Thing.ThingName}/commands'),
          },
          {
            Effect: 'Allow',
            Action: 'iot:Receive',
            Resource: arn('topic/katooni/${iot:Connection.Thing.ThingName}/commands'),
          },
        ],
      },
    });

    for (const name of deviceNames) {
      const thingName = `katooni-${deployEnv}-${name}`;
      new iot.CfnThing(this, `Thing-${name}`, {
        thingName,
        // Read by Fleet: ota-signed makes it refuse unsigned builds for this device. No ota-approval:
        // Katooni installs a signed, newer image without asking.
        attributePayload: {
          attributes: { project: 'katooni', deviceType: 'cable-tester', capabilities: 'ota,ota-signed' },
        },
      });
      new cdk.CfnOutput(this, `ThingName-${name}`, { value: thingName });
    }
    new cdk.CfnOutput(this, 'DevicePolicyName', { value: policy.policyName! });
  }
}
